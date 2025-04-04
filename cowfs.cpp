#include "cowfs.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace cowfs {

COWFileSystem::COWFileSystem(const std::string& disk_path, size_t disk_size)
    : disk_path(disk_path), disk_size(disk_size) {

    total_blocks = disk_size / BLOCK_SIZE;
    if (disk_size % BLOCK_SIZE != 0) {
        total_blocks++;
    }

    // Inicializar estructuras de datos
    file_descriptors.resize(MAX_FILES);
    inodes.resize(MAX_FILES);
    blocks.resize(total_blocks);

    init_file_system();

    if (!initialize_disk()) {
        throw std::runtime_error("Failed to initialize disk");
    }
}

COWFileSystem::~COWFileSystem() {
    save_to_disk();
}

void COWFileSystem::save_to_disk() {
    std::ofstream disk(disk_path, std::ios::binary);
    if (!disk.is_open()) return;

    // Serializar inodos
    for (const auto& inode : inodes) {
        disk.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));

        // Serializar version_history
        size_t history_size = inode.version_history.size();
        disk.write(reinterpret_cast<const char*>(&history_size), sizeof(size_t));

        for (const auto& version : inode.version_history) {
            disk.write(reinterpret_cast<const char*>(&version), sizeof(VersionInfo));
            size_t ts_size = version.timestamp.size();
            disk.write(reinterpret_cast<const char*>(&ts_size), sizeof(size_t));
            disk.write(version.timestamp.data(), ts_size);
        }
    }

    // Serializar bloques
    disk.write(reinterpret_cast<const char*>(blocks.data()), blocks.size() * sizeof(Block));
}

bool COWFileSystem::initialize_disk() {
    std::ifstream disk(disk_path, std::ios::binary);
    if (disk.is_open()) {
        // Cargar estado existente
        for (auto& inode : inodes) {
            disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

            // Cargar version_history
            size_t history_size;
            disk.read(reinterpret_cast<char*>(&history_size), sizeof(size_t));
            inode.version_history.resize(history_size);

            for (auto& version : inode.version_history) {
                disk.read(reinterpret_cast<char*>(&version), sizeof(VersionInfo));
                size_t ts_size;
                disk.read(reinterpret_cast<char*>(&ts_size), sizeof(size_t));
                version.timestamp.resize(ts_size);
                disk.read(&version.timestamp[0], ts_size);
            }

            // Actualizar mapa de nombres
            if (inode.is_used) {
                filename_to_inode[inode.filename] = &inode - &inodes[0];
            }
        }

        // Cargar bloques
        disk.read(reinterpret_cast<char*>(blocks.data()), blocks.size() * sizeof(Block));
        return true;
    }

    // Crear nuevo archivo de disco
    std::ofstream new_disk(disk_path, std::ios::binary);
    if (!new_disk.is_open()) {
        return false;
    }

    // Inicializar disco vacío
    init_file_system();
    save_to_disk();
    return true;
}

void COWFileSystem::init_file_system() {
    // Inicializar descriptores de archivo
    for (auto& fd : file_descriptors) {
        fd = FileDescriptor();
    }

    // Inicializar inodos
    for (auto& inode : inodes) {
        inode = Inode();
    }

    // Inicializar bloques
    for (auto& block : blocks) {
        block = Block();
    }

    // Inicializar listas libres
    free_blocks.clear();
    for (size_t i = 0; i < blocks.size(); ++i) {
        free_blocks.push_back(i);
    }

    free_fds.clear();
    for (fd_t i = 0; i < static_cast<fd_t>(file_descriptors.size()); ++i) {
        free_fds.push_back(i);
    }

    filename_to_inode.clear();
}

fd_t COWFileSystem::create(const std::string& filename) {
    if (filename.empty() || filename.length() >= MAX_FILENAME_LENGTH) {
        return INVALID_FD;
    }

    // Verificar si el archivo ya existe
    if (find_inode(filename) != nullptr) {
        return INVALID_FD;
    }

    // Encontrar inodo libre
    size_t inode_index;
    for (inode_index = 0; inode_index < inodes.size(); ++inode_index) {
        if (!inodes[inode_index].is_used) break;
    }

    if (inode_index >= inodes.size()) {
        return INVALID_FD;
    }

    // Inicializar inodo
    auto& inode = inodes[inode_index];
    strncpy(inode.filename, filename.c_str(), MAX_FILENAME_LENGTH - 1);
    inode.filename[MAX_FILENAME_LENGTH - 1] = '\0';
    inode.is_used = true;
    filename_to_inode[filename] = inode_index;

    // Asignar descriptor de archivo
    fd_t fd = allocate_file_descriptor();
    if (fd == INVALID_FD) {
        inode.is_used = false;
        filename_to_inode.erase(filename);
        return INVALID_FD;
    }

    file_descriptors[fd].inode = &inode;
    file_descriptors[fd].mode = FileMode::WRITE;
    file_descriptors[fd].current_position = 0;
    file_descriptors[fd].is_valid = true;

    return fd;
}

fd_t COWFileSystem::open(const std::string& filename, FileMode mode) {
    Inode* inode = find_inode(filename);
    if (!inode) {
        return INVALID_FD;
    }

    fd_t fd = allocate_file_descriptor();
    if (fd == INVALID_FD) {
        return INVALID_FD;
    }

    file_descriptors[fd].inode = inode;
    file_descriptors[fd].mode = mode;
    file_descriptors[fd].is_valid = true;

    if (mode == FileMode::WRITE) {
        file_descriptors[fd].current_position = inode->size;
    } else {
        file_descriptors[fd].current_position = 0;
    }

    return fd;
}

ssize_t COWFileSystem::read(fd_t fd, void* buffer, size_t size) {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
        !file_descriptors[fd].is_valid || buffer == nullptr) {
        return -1;
    }

    auto& fd_entry = file_descriptors[fd];
    if (fd_entry.mode != FileMode::READ) {
        return -1;
    }

    // Verificar límites
    size_t remaining = fd_entry.inode->size - fd_entry.current_position;
    size = std::min(size, remaining);

    size_t bytes_read = 0;
    size_t current_block = fd_entry.inode->first_block;
    size_t block_offset = fd_entry.current_position % BLOCK_SIZE;

    while (bytes_read < size && current_block != 0) {
        size_t bytes_to_read = std::min(size - bytes_read, BLOCK_SIZE - block_offset);
        memcpy(static_cast<uint8_t*>(buffer) + bytes_read,
               blocks[current_block].data + block_offset,
               bytes_to_read);

        bytes_read += bytes_to_read;
        block_offset = 0;
        current_block = blocks[current_block].next_block;
    }

    fd_entry.current_position += bytes_read;
    return bytes_read;
}

ssize_t COWFileSystem::write(fd_t fd, const void* buffer, size_t size) {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
        !file_descriptors[fd].is_valid || buffer == nullptr) {
        return -1;
    }

    auto& fd_entry = file_descriptors[fd];
    if (fd_entry.mode != FileMode::WRITE) {
        return -1;
    }

    // Asignar primer bloque si es necesario
    size_t new_first_block;
    if (fd_entry.inode->first_block == 0) {
        if (!allocate_block(new_first_block)) {
            return -1;
        }
    } else {
        if (!copy_block(fd_entry.inode->first_block, new_first_block)) {
            return -1;
        }
    }

    // Escribir datos
    size_t bytes_written = 0;
    size_t current_block = new_first_block;
    size_t block_offset = fd_entry.current_position % BLOCK_SIZE;

    while (bytes_written < size) {
        if (block_offset == 0 && bytes_written > 0) {
            size_t next_block;
            if (!allocate_block(next_block)) {
                free_block_chain(new_first_block);
                return -1;
            }
            blocks[current_block].next_block = next_block;
            current_block = next_block;
        }

        size_t bytes_to_write = std::min(size - bytes_written, BLOCK_SIZE - block_offset);
        memcpy(blocks[current_block].data + block_offset,
               static_cast<const uint8_t*>(buffer) + bytes_written,
               bytes_to_write);

        bytes_written += bytes_to_write;
        block_offset = (block_offset + bytes_to_write) % BLOCK_SIZE;
    }

    // Actualizar información de versión
    VersionInfo new_version;
    new_version.version_number = fd_entry.inode->version_count + 1;
    new_version.block_index = new_first_block;
    new_version.size = fd_entry.current_position + bytes_written;
    new_version.timestamp = get_current_timestamp();

    // Actualizar inodo
    fd_entry.inode->version_history.push_back(new_version);
    fd_entry.inode->first_block = new_first_block;
    fd_entry.inode->size = fd_entry.current_position + bytes_written;
    fd_entry.inode->version_count++;
    fd_entry.current_position += bytes_written;

    return bytes_written;
}

int COWFileSystem::close(fd_t fd) {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
        !file_descriptors[fd].is_valid) {
        return -1;
    }

    file_descriptors[fd].is_valid = false;
    free_file_descriptor(fd);
    return 0;
}

bool COWFileSystem::remove(const std::string& filename) {
    Inode* inode = find_inode(filename);
    if (!inode || !inode->is_used) {
        return false;
    }

    // Liberar bloques
    free_block_chain(inode->first_block);

    // Limpiar inodo
    inode->is_used = false;
    inode->first_block = 0;
    inode->size = 0;
    inode->version_count = 0;
    inode->version_history.clear();

    // Eliminar del mapa
    filename_to_inode.erase(filename);
    return true;
}

Inode* COWFileSystem::find_inode(const std::string& filename) {
    auto it = filename_to_inode.find(filename);
    if (it != filename_to_inode.end()) {
        return &inodes[it->second];
    }
    return nullptr;
}

fd_t COWFileSystem::allocate_file_descriptor() {
    if (!free_fds.empty()) {
        fd_t fd = free_fds.back();
        free_fds.pop_back();
        return fd;
    }

    // Búsqueda tradicional si no hay libres
    for (size_t i = 0; i < file_descriptors.size(); ++i) {
        if (!file_descriptors[i].is_valid) {
            return static_cast<fd_t>(i);
        }
    }
    return INVALID_FD;
}

void COWFileSystem::free_file_descriptor(fd_t fd) {
    if (fd >= 0 && fd < static_cast<fd_t>(file_descriptors.size())) {
        free_fds.push_back(fd);
    }
}

bool COWFileSystem::allocate_block(size_t& block_index) {
    if (!free_blocks.empty()) {
        block_index = free_blocks.back();
        free_blocks.pop_back();
        blocks[block_index].is_used = true;
        blocks[block_index].next_block = 0;
        return true;
    }

    // Búsqueda tradicional si no hay libres
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!blocks[i].is_used) {
            block_index = i;
            blocks[i].is_used = true;
            blocks[i].next_block = 0;
            return true;
        }
    }
    return false;
}

void COWFileSystem::free_block(size_t block_index) {
    if (block_index < blocks.size()) {
        blocks[block_index].is_used = false;
        blocks[block_index].next_block = 0;
        free_blocks.push_back(block_index);
    }
}

bool COWFileSystem::copy_block(size_t source_block, size_t& dest_block) {
    if (!allocate_block(dest_block)) {
        return false;
    }

    if (source_block != 0) {
        memcpy(blocks[dest_block].data, blocks[source_block].data, BLOCK_SIZE);
        blocks[dest_block].next_block = blocks[source_block].next_block;
    }

    return true;
}

void COWFileSystem::free_block_chain(size_t block_index) {
    while (block_index != 0) {
        size_t next = blocks[block_index].next_block;
        free_block(block_index);
        block_index = next;
    }
}

std::vector<VersionInfo> COWFileSystem::get_version_history(fd_t fd) const {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
        !file_descriptors[fd].is_valid) {
        return std::vector<VersionInfo>();
    }
    return file_descriptors[fd].inode->version_history;
}

size_t COWFileSystem::get_version_count(fd_t fd) const {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
        !file_descriptors[fd].is_valid) {
        return 0;
    }
    return file_descriptors[fd].inode->version_count;
}

bool COWFileSystem::revert_to_version(fd_t fd, size_t version) {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
        !file_descriptors[fd].is_valid) {
        return false;
    }

    auto& fd_entry = file_descriptors[fd];
    if (fd_entry.mode != FileMode::WRITE) {
        return false;
    }

    // Buscar la versión solicitada
    const VersionInfo* target_version = nullptr;
    for (const auto& ver : fd_entry.inode->version_history) {
        if (ver.version_number == version) {
            target_version = &ver;
            break;
        }
    }

    if (!target_version) return false;

    // Crear nueva versión basada en la solicitada
    size_t new_first_block;
    if (!copy_block(target_version->block_index, new_first_block)) {
        return false;
    }

    // Actualizar metadatos
    VersionInfo new_version;
    new_version.version_number = fd_entry.inode->version_count + 1;
    new_version.block_index = new_first_block;
    new_version.size = target_version->size;
    new_version.timestamp = get_current_timestamp();

    fd_entry.inode->version_history.push_back(new_version);
    fd_entry.inode->first_block = new_first_block;
    fd_entry.inode->size = target_version->size;
    fd_entry.inode->version_count++;
    fd_entry.current_position = target_version->size;

    return true;
}

bool COWFileSystem::list_files(std::vector<std::string>& files) const {
    files.clear();
    for (const auto& pair : filename_to_inode) {
        files.push_back(pair.first);
    }
    return true;
}

size_t COWFileSystem::get_file_size(fd_t fd) const {
    if (fd == INVALID_FD || fd >= static_cast<fd_t>(file_descriptors.size()) ||
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
    // Versión simple: solo liberar bloques no usados
    for (auto& block : blocks) {
        if (!block.is_used) {
            block.next_block = 0;
            memset(block.data, 0, BLOCK_SIZE);
        }
    }
}

} // namespace cowfs