#include "cowfs.hpp"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>  // Para std::find_if

namespace cowfs {

COWFileSystem::COWFileSystem(const std::string& disk_path, size_t disk_size)
    : disk_path(disk_path), disk_size(disk_size), free_blocks_list(nullptr) {
    std::cout << "Initializing file system with size: " << disk_size << " bytes" << std::endl;
    
    total_blocks = disk_size / BLOCK_SIZE;
    std::cout << "Total blocks: " << total_blocks << std::endl;
    
    // Resize containers
    file_descriptors.resize(MAX_FILES);
    inodes.resize(MAX_FILES);
    blocks.resize(total_blocks);

    // Initialize all data structures
    init_file_system();

    std::cout << "File system initialized with:" << std::endl
              << "  Max files: " << MAX_FILES << std::endl
              << "  Block size: " << BLOCK_SIZE << " bytes" << std::endl;

    // Inicializar la lista de bloques libres con todo el espacio disponible
    add_to_free_list(0, total_blocks);

    if (!initialize_disk()) {
        throw std::runtime_error("Failed to initialize disk");
    }
}

COWFileSystem::~COWFileSystem() {
    // Limpiar la lista de bloques libres
    while (free_blocks_list) {
        FreeBlockInfo* temp = free_blocks_list;
        free_blocks_list = free_blocks_list->next;
        delete temp;
    }

    // Save current state to disk
    std::ofstream disk(disk_path, std::ios::binary);
    if (disk.is_open()) {
        // Write inodes
        disk.write(reinterpret_cast<char*>(inodes.data()), inodes.size() * sizeof(Inode));
        // Write blocks
        disk.write(reinterpret_cast<char*>(blocks.data()), blocks.size() * sizeof(Block));
    }
}

bool COWFileSystem::initialize_disk() {
    std::ifstream disk(disk_path, std::ios::binary);
    if (disk.is_open()) {
        // Load existing state
        disk.read(reinterpret_cast<char*>(inodes.data()), inodes.size() * sizeof(Inode));
        disk.read(reinterpret_cast<char*>(blocks.data()), blocks.size() * sizeof(Block));
        return true;
    } else {
        // Create new disk file
        std::ofstream new_disk(disk_path, std::ios::binary);
        if (!new_disk.is_open()) {
            return false;
        }
        
        // Initialize all inodes as unused
        for (auto& inode : inodes) {
            inode.is_used = false;
            inode.filename[0] = '\0';  // Explicitly clear filename
            inode.first_block = 0;
            inode.size = 0;
            inode.version_count = 0;
        }

        // Initialize all blocks as unused
        for (auto& block : blocks) {
            block.is_used = false;
            block.next_block = 0;
            std::memset(block.data, 0, BLOCK_SIZE);
        }

        // Write the initialized state to disk
        new_disk.write(reinterpret_cast<char*>(inodes.data()), inodes.size() * sizeof(Inode));
        new_disk.write(reinterpret_cast<char*>(blocks.data()), blocks.size() * sizeof(Block));
        return true;
    }
}

fd_t COWFileSystem::create(const std::string& filename) {
    if (filename.length() >= MAX_FILENAME_LENGTH) {
        std::cerr << "Error: Filename too long" << std::endl;
        return -1;
    }

    // Check if file already exists
    if (find_inode(filename) != nullptr) {
        std::cerr << "Error: File already exists" << std::endl;
        return -1;
    }

    // Find free inode
    Inode* inode = nullptr;
    for (auto& i : inodes) {
        if (!i.is_used) {
            inode = &i;
            break;
        }
    }
    if (!inode) {
        std::cerr << "Error: No free inodes available" << std::endl;
        return -1;
    }

    // Initialize inode
    std::memset(inode, 0, sizeof(Inode));
    std::strncpy(inode->filename, filename.c_str(), MAX_FILENAME_LENGTH - 1);
    inode->filename[MAX_FILENAME_LENGTH - 1] = '\0';
    inode->first_block = 0;
    inode->size = 0;
    inode->version_count = 0;  // Start at 0, first write will make it 1
    inode->is_used = true;
    inode->version_history.clear();

    // Allocate file descriptor
    fd_t fd = allocate_file_descriptor();
    if (fd < 0) {
        std::cerr << "Error: Failed to allocate file descriptor" << std::endl;
        inode->is_used = false;  // Rollback inode allocation
        return -1;
    }

    file_descriptors[fd].inode = inode;
    file_descriptors[fd].mode = FileMode::WRITE;
    file_descriptors[fd].current_position = 0;
    file_descriptors[fd].is_valid = true;

    std::cout << "Successfully created file with fd: " << fd << std::endl;
    return fd;
}

fd_t COWFileSystem::open(const std::string& filename, FileMode mode) {
    Inode* inode = find_inode(filename);
    if (!inode) {
        std::cerr << "File not found in open" << std::endl;
        return -1;
    }

    fd_t fd = allocate_file_descriptor();
    if (fd < 0) {
        std::cerr << "Failed to allocate file descriptor in open" << std::endl;
        return -1;
    }

    // Initialize file descriptor
    file_descriptors[fd].inode = inode;
    file_descriptors[fd].mode = mode;
    file_descriptors[fd].is_valid = true;

    // For write mode, position at the end of file for appending
    if (mode == FileMode::WRITE) {
        file_descriptors[fd].current_position = inode->size;
    } else {
        file_descriptors[fd].current_position = 0;
    }

    std::cout << "Successfully opened file with fd: " << fd 
              << ", mode: " << (mode == FileMode::WRITE ? "WRITE" : "READ")
              << ", current_position: " << file_descriptors[fd].current_position 
              << std::endl;

    return fd;
}

ssize_t COWFileSystem::read(fd_t fd, void* buffer, size_t size) {
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        return -1;
    }

    auto& fd_entry = file_descriptors[fd];
    if (fd_entry.mode != FileMode::READ) {
        return -1;
    }

    size_t bytes_read = 0;
    size_t current_block = fd_entry.inode->first_block;
    size_t block_offset = fd_entry.current_position % BLOCK_SIZE;

    while (bytes_read < size && current_block != 0) {
        size_t bytes_to_read = std::min(size - bytes_read, BLOCK_SIZE - block_offset);
        std::memcpy(static_cast<uint8_t*>(buffer) + bytes_read,
                   blocks[current_block].data + block_offset,
                   bytes_to_read);
        
        bytes_read += bytes_to_read;
        block_offset = 0;
        current_block = blocks[current_block].next_block;
    }

    fd_entry.current_position += bytes_read;
    return bytes_read;
}

std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool COWFileSystem::find_delta(const void* old_data, const void* new_data,
                             size_t old_size, size_t new_size,
                             size_t& delta_start, size_t& delta_size) {
    const uint8_t* old_bytes = static_cast<const uint8_t*>(old_data);
    const uint8_t* new_bytes = static_cast<const uint8_t*>(new_data);
    
    // Si los datos son idénticos, no hay delta
    if (old_size == new_size && std::memcmp(old_data, new_data, old_size) == 0) {
        delta_start = 0;
        delta_size = 0;
        return true;
    }
    
    // Encontrar dónde comienzan las diferencias
    delta_start = 0;
    while (delta_start < old_size && delta_start < new_size &&
           old_bytes[delta_start] == new_bytes[delta_start]) {
        delta_start++;
    }
    
    // Si el nuevo contenido es más corto y no hay diferencias hasta aquí
    if (delta_start == new_size && new_size < old_size) {
        delta_size = 0;
        return true;
    }
    
    // Si el nuevo contenido es más largo pero igual hasta el final del viejo
    if (delta_start == old_size && new_size > old_size) {
        delta_size = new_size - old_size;
        return true;
    }
    
    // Encontrar dónde terminan las diferencias desde el final
    size_t common_suffix = 0;
    while (common_suffix < (old_size - delta_start) && 
           common_suffix < (new_size - delta_start) &&
           old_bytes[old_size - 1 - common_suffix] == new_bytes[new_size - 1 - common_suffix]) {
        common_suffix++;
    }
    
    // Calcular el tamaño del delta
    delta_size = (new_size - delta_start) - common_suffix;
    
    // Validación final
    if (delta_start + delta_size > new_size) {
        delta_size = new_size - delta_start;
    }
    
    return true;
}

bool COWFileSystem::write_delta_blocks(const void* buffer, size_t size,
                                     size_t delta_start, size_t& first_block) {
    if (size == 0 || delta_start >= size) {
        first_block = 0;
        return true;
    }
    
    // Calcular cuántos bloques necesitamos
    size_t actual_size = std::min(size - delta_start, size);
    size_t blocks_needed = (actual_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    first_block = 0;
    size_t current_block = 0;
    size_t prev_block = 0;
    
    const uint8_t* data = static_cast<const uint8_t*>(buffer) + delta_start;
    size_t remaining = actual_size;
    
    while (remaining > 0) {
        if (!allocate_block(current_block)) {
            // Liberar bloques si falla
            if (first_block != 0) {
                size_t block = first_block;
                while (block != 0) {
                    size_t next = blocks[block].next_block;
                    free_block(block);
                    block = next;
                }
            }
            return false;
        }
        
        if (first_block == 0) {
            first_block = current_block;
        } else {
            blocks[prev_block].next_block = current_block;
        }
        
        size_t bytes_to_write = std::min(remaining, BLOCK_SIZE);
        std::memcpy(blocks[current_block].data, data, bytes_to_write);
        
        // Inicializar el resto del bloque con ceros si es necesario
        if (bytes_to_write < BLOCK_SIZE) {
            std::memset(blocks[current_block].data + bytes_to_write, 0, BLOCK_SIZE - bytes_to_write);
        }
        
        data += bytes_to_write;
        remaining -= bytes_to_write;
        prev_block = current_block;
    }
    
    // Asegurar que el último bloque tenga next_block = 0
    if (prev_block < blocks.size()) {
        blocks[prev_block].next_block = 0;
    }
    
    return true;
}

void COWFileSystem::increment_block_refs(size_t block_index) {
    while (block_index != 0 && block_index < blocks.size()) {
        blocks[block_index].ref_count++;
        block_index = blocks[block_index].next_block;
    }
}

void COWFileSystem::decrement_block_refs(size_t block_index) {
    while (block_index != 0 && block_index < blocks.size()) {
        if (blocks[block_index].ref_count > 0) {
            blocks[block_index].ref_count--;
            if (blocks[block_index].ref_count == 0) {
                size_t next_block = blocks[block_index].next_block;
                free_block(block_index);
                block_index = next_block;
            } else {
                break; // Si aún hay referencias, no seguir
            }
        }
        block_index = blocks[block_index].next_block;
    }
}

ssize_t COWFileSystem::write(fd_t fd, const void* buffer, size_t size) {
    std::cout << "Starting write operation for fd: " << fd << std::endl;
    
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        std::cerr << "Invalid file descriptor in write" << std::endl;
        return -1;
    }
    
    auto& fd_entry = file_descriptors[fd];
    if (fd_entry.mode != FileMode::WRITE) {
        std::cerr << "File not opened for writing" << std::endl;
        return -1;
    }
    
    if (!fd_entry.inode) {
        std::cerr << "No inode associated with file descriptor" << std::endl;
        return -1;
    }
    
    // Obtener datos de la versión anterior si existe
    std::vector<uint8_t> old_data;
    size_t old_size = 0;
    
    if (fd_entry.inode->version_count > 0) {
        const auto& last_version = fd_entry.inode->version_history.back();
        old_size = last_version.size;
        old_data.resize(old_size);
        
        size_t current_block = last_version.block_index;
        size_t pos = 0;
        
        while (current_block != 0 && pos < old_size) {
            size_t bytes_to_read = std::min(old_size - pos, BLOCK_SIZE);
            std::memcpy(old_data.data() + pos, blocks[current_block].data, bytes_to_read);
            pos += bytes_to_read;
            current_block = blocks[current_block].next_block;
        }
    }
    
    // Encontrar el delta
    size_t delta_start = 0;
    size_t delta_size = size;
    
    if (!old_data.empty()) {
        find_delta(old_data.data(), buffer, old_size, size, delta_start, delta_size);
    }
    
    // Escribir solo los bloques que contienen cambios
    size_t new_first_block = 0;
    if (delta_size > 0) {
        if (!write_delta_blocks(buffer, delta_size, delta_start, new_first_block)) {
            std::cerr << "Failed to write delta blocks" << std::endl;
            return -1;
        }
    }
    
    // Crear nueva versión
    VersionInfo new_version;
    new_version.version_number = fd_entry.inode->version_count + 1;
    new_version.block_index = new_first_block;
    new_version.size = size;
    new_version.timestamp = get_current_timestamp();
    new_version.delta_start = delta_start;
    new_version.delta_size = delta_size;
    new_version.prev_version = fd_entry.inode->version_count > 0 ? 
                              fd_entry.inode->version_count : 0;
    
    // Incrementar referencias a bloques compartidos
    if (fd_entry.inode->version_count > 0) {
        increment_block_refs(fd_entry.inode->first_block);
    }
    
    // Actualizar inode
    fd_entry.inode->version_history.push_back(new_version);
    fd_entry.inode->first_block = new_first_block;
    fd_entry.inode->size = size;
    fd_entry.inode->version_count++;
    
    std::cout << "Write operation completed:"
              << "\n  bytes written: " << size
              << "\n  delta size: " << delta_size
              << "\n  new version: " << fd_entry.inode->version_count
              << "\n  new size: " << fd_entry.inode->size
              << std::endl;
    
    return size;
}

int COWFileSystem::close(fd_t fd) {
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        return -1;
    }

    file_descriptors[fd].is_valid = false;
    return 0;
}

// Helper functions implementation
Inode* COWFileSystem::find_inode(const std::string& filename) {
    for (size_t i = 0; i < inodes.size(); i++) {
        if (inodes[i].is_used) {
            // Debug output to check what's happening
            std::cout << "Checking inode " << i << ": " 
                     << "used=" << inodes[i].is_used 
                     << ", filename='" << inodes[i].filename << "'" << std::endl;
            
            if (std::strcmp(inodes[i].filename, filename.c_str()) == 0) {
                return &inodes[i];
            }
        }
    }
    return nullptr;
}

fd_t COWFileSystem::allocate_file_descriptor() {
    for (size_t i = 0; i < file_descriptors.size(); ++i) {
        if (!file_descriptors[i].is_valid) {
            return static_cast<fd_t>(i);
        }
    }
    return -1;
}

void COWFileSystem::free_file_descriptor(fd_t fd) {
    if (fd >= 0 && fd < static_cast<fd_t>(file_descriptors.size())) {
        file_descriptors[fd].is_valid = false;
    }
}

bool COWFileSystem::allocate_block(size_t& block_index) {
    // Buscar el mejor bloque libre que se ajuste
    FreeBlockInfo* best_block = find_best_fit(1);
    
    if (best_block) {
        block_index = best_block->start_block;
        
        // Actualizar la lista de bloques libres
        if (best_block->block_count > 1) {
            best_block->start_block++;
            best_block->block_count--;
        } else {
            if (best_block == free_blocks_list) {
                free_blocks_list = best_block->next;
            } else {
                FreeBlockInfo* current = free_blocks_list;
                while (current->next != best_block) {
                    current = current->next;
                }
                current->next = best_block->next;
            }
            delete best_block;
        }
        
        // Inicializar el bloque
        blocks[block_index].is_used = true;
        blocks[block_index].next_block = 0;
        return true;
    }
    
    return false;
}

void COWFileSystem::free_block(size_t block_index) {
    if (block_index < blocks.size()) {
        blocks[block_index].is_used = false;
        blocks[block_index].next_block = 0;
    }
}

bool COWFileSystem::copy_block(size_t source_block, size_t& dest_block) {
    if (!allocate_block(dest_block)) {
        return false;
    }

    if (source_block != 0) {
        std::memcpy(blocks[dest_block].data, blocks[source_block].data, BLOCK_SIZE);
        blocks[dest_block].next_block = blocks[source_block].next_block;
    }

    return true;
}

// Version management implementation
std::vector<VersionInfo> COWFileSystem::get_version_history(fd_t fd) const {
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        return std::vector<VersionInfo>();
    }
    return file_descriptors[fd].inode->version_history;
}

size_t COWFileSystem::get_version_count(fd_t fd) const {
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        return 0;
    }
    return file_descriptors[fd].inode->version_count;
}

bool COWFileSystem::revert_to_version(fd_t fd, size_t version) {
    // This is a simplified version. In a real implementation, you would need to
    // maintain a version history and implement proper version tracking.
    return false;
}

bool COWFileSystem::rollback_to_version(fd_t fd, size_t version_number) {
    // Verificar que el descriptor de archivo sea válido
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        std::cerr << "Error: Descriptor de archivo inválido" << std::endl;
        return false;
    }

    auto& fd_entry = file_descriptors[fd];
    if (!fd_entry.inode) {
        std::cerr << "Error: No hay inodo asociado al descriptor" << std::endl;
        return false;
    }

    // Verificar que la versión solicitada exista
    if (version_number == 0 || version_number > fd_entry.inode->version_count) {
        std::cerr << "Error: Versión " << version_number << " no existe" << std::endl;
        return false;
    }

    // Encontrar la versión solicitada en el historial
    auto it = std::find_if(fd_entry.inode->version_history.begin(),
                          fd_entry.inode->version_history.end(),
                          [version_number](const VersionInfo& v) {
                              return v.version_number == version_number;
                          });

    if (it == fd_entry.inode->version_history.end()) {
        std::cerr << "Error: No se encontró la versión " << version_number << std::endl;
        return false;
    }

    // Crear una nueva versión basada en la versión seleccionada
    VersionInfo new_version;
    new_version.version_number = fd_entry.inode->version_count + 1;
    new_version.block_index = it->block_index;
    new_version.size = it->size;
    new_version.timestamp = get_current_timestamp();
    new_version.delta_start = 0;
    new_version.delta_size = it->size;
    new_version.prev_version = version_number;

    // Incrementar las referencias a los bloques de la versión seleccionada
    increment_block_refs(it->block_index);

    // Actualizar el inodo con la nueva versión
    fd_entry.inode->version_history.push_back(new_version);
    fd_entry.inode->first_block = it->block_index;
    fd_entry.inode->size = it->size;
    fd_entry.inode->version_count++;

    std::cout << "Rollback exitoso a la versión " << version_number << std::endl;
    return true;
}

// File system operations implementation
bool COWFileSystem::list_files(std::vector<std::string>& files) const {
    files.clear();
    for (const auto& inode : inodes) {
        if (inode.is_used) {
            files.push_back(inode.filename);
        }
    }
    return true;
}

size_t COWFileSystem::get_file_size(fd_t fd) const {
    if (fd < 0 || fd >= static_cast<fd_t>(file_descriptors.size()) || 
        !file_descriptors[fd].is_valid) {
        return 0;
    }
    return file_descriptors[fd].inode->size;
}

FileStatus COWFileSystem::get_file_status(fd_t fd) const {
    FileStatus status = {false, false, 0, 0};
    if (fd >= 0 && fd < static_cast<fd_t>(file_descriptors.size()) && 
        file_descriptors[fd].is_valid) {
        status.is_open = true;
        status.is_modified = (file_descriptors[fd].mode == FileMode::WRITE);
        status.current_size = file_descriptors[fd].inode->size;
        status.current_version = file_descriptors[fd].inode->version_count;
    }
    return status;
}

// Memory management implementation
size_t COWFileSystem::get_total_memory_usage() const {
    size_t total = 0;
    for (const auto& block : blocks) {
        if (block.is_used) {
            total += BLOCK_SIZE;
        }
    }
    return total;
}

void COWFileSystem::garbage_collect() {
    std::vector<bool> block_used(blocks.size(), false);
    
    // Marcar bloques en uso
    for (const auto& inode : inodes) {
        if (inode.is_used) {
            for (const auto& version : inode.version_history) {
                size_t current_block = version.block_index;
                while (current_block != 0 && current_block < blocks.size()) {
                    if (blocks[current_block].ref_count > 0) {
                        block_used[current_block] = true;
                    }
                    current_block = blocks[current_block].next_block;
                }
            }
        }
    }
    
    // Encontrar bloques libres contiguos
    size_t start = 0;
    while (start < blocks.size()) {
        if (!block_used[start]) {
            size_t count = 0;
            while (start + count < blocks.size() && !block_used[start + count]) {
                blocks[start + count].is_used = false;
                blocks[start + count].next_block = 0;
                blocks[start + count].ref_count = 0;
                std::memset(blocks[start + count].data, 0, BLOCK_SIZE);
                count++;
            }
            
            if (count > 0) {
                add_to_free_list(start, count);
            }
            
            start += count;
        }
        start++;
    }
    
    merge_free_blocks();
}

void COWFileSystem::init_file_system() {
    // Initialize all file descriptors
    for (auto& fd : file_descriptors) {
        fd.inode = nullptr;
        fd.mode = FileMode::READ;
        fd.current_position = 0;
        fd.is_valid = false;
    }

    // Initialize all inodes
    for (auto& inode : inodes) {
        inode.is_used = false;
        std::memset(inode.filename, 0, MAX_FILENAME_LENGTH);
        inode.first_block = 0;
        inode.size = 0;
        inode.version_count = 0;
        inode.version_history.clear();
        inode.shared_blocks.clear();
    }

    // Initialize all blocks
    for (auto& block : blocks) {
        block.is_used = false;
        block.next_block = 0;
        block.ref_count = 0;
        std::memset(block.data, 0, BLOCK_SIZE);
    }
}

bool COWFileSystem::merge_free_blocks() {
    if (!free_blocks_list) return false;
    
    bool merged = false;
    FreeBlockInfo* current = free_blocks_list;
    
    while (current && current->next) {
        if (current->start_block + current->block_count == current->next->start_block) {
            // Fusionar bloques contiguos
            current->block_count += current->next->block_count;
            FreeBlockInfo* temp = current->next;
            current->next = current->next->next;
            delete temp;
            merged = true;
        } else {
            current = current->next;
        }
    }
    
    return merged;
}

bool COWFileSystem::split_free_block(FreeBlockInfo* block, size_t size_needed) {
    if (!block || block->block_count < size_needed) return false;
    
    if (block->block_count > size_needed) {
        // Crear nuevo bloque con el espacio restante
        FreeBlockInfo* new_block = new FreeBlockInfo{
            block->start_block + size_needed,
            block->block_count - size_needed,
            block->next
        };
        
        block->block_count = size_needed;
        block->next = new_block;
    }
    
    return true;
}

void COWFileSystem::add_to_free_list(size_t start, size_t count) {
    FreeBlockInfo* new_block = new FreeBlockInfo{start, count, nullptr};
    
    if (!free_blocks_list || start < free_blocks_list->start_block) {
        new_block->next = free_blocks_list;
        free_blocks_list = new_block;
    } else {
        FreeBlockInfo* current = free_blocks_list;
        while (current->next && current->next->start_block < start) {
            current = current->next;
        }
        new_block->next = current->next;
        current->next = new_block;
    }
    
    merge_free_blocks();
}

FreeBlockInfo* COWFileSystem::find_best_fit(size_t blocks_needed) {
    FreeBlockInfo* best_fit = nullptr;
    FreeBlockInfo* current = free_blocks_list;
    size_t smallest_difference = SIZE_MAX;
    
    while (current) {
        if (current->block_count >= blocks_needed) {
            size_t difference = current->block_count - blocks_needed;
            if (difference < smallest_difference) {
                smallest_difference = difference;
                best_fit = current;
                if (difference == 0) break; // Encontramos un ajuste perfecto
            }
        }
        current = current->next;
    }
    
    return best_fit;
}

} // namespace cowfs 