#include "cowfs.hpp"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace cowfs {

COWFileSystem::COWFileSystem(const std::string& disk_path, size_t disk_size)
    : disk_path(disk_path), disk_size(disk_size) {
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

    if (!initialize_disk()) {
        throw std::runtime_error("Failed to initialize disk");
    }
}

COWFileSystem::~COWFileSystem() {
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

    std::cout << "Current file state before write:"
              << "\n  inode first_block: " << fd_entry.inode->first_block
              << "\n  inode size: " << fd_entry.inode->size
              << "\n  inode version_count: " << fd_entry.inode->version_count
              << "\n  current_position: " << fd_entry.current_position
              << std::endl;

    // Allocate first block if needed
    size_t new_first_block;
    if (fd_entry.inode->first_block == 0) {
        if (!allocate_block(new_first_block)) {
            std::cerr << "Failed to allocate first block" << std::endl;
            return -1;
        }
        std::cout << "Allocated first block: " << new_first_block << std::endl;
    } else {
        if (!copy_block(fd_entry.inode->first_block, new_first_block)) {
            std::cerr << "Failed to copy existing block" << std::endl;
            return -1;
        }
        std::cout << "Copied existing block: " << fd_entry.inode->first_block 
                  << " to " << new_first_block << std::endl;
    }

    // Write data
    size_t bytes_written = 0;
    size_t current_block = new_first_block;
    size_t block_offset = fd_entry.current_position % BLOCK_SIZE;

    while (bytes_written < size) {
        if (block_offset == 0 && bytes_written > 0) {
            size_t next_block;
            if (!allocate_block(next_block)) {
                std::cerr << "Failed to allocate next block" << std::endl;
                free_block(new_first_block);
                return -1;
            }
            blocks[current_block].next_block = next_block;
            current_block = next_block;
            std::cout << "Allocated additional block: " << next_block << std::endl;
        }

        size_t bytes_to_write = std::min(size - bytes_written, BLOCK_SIZE - block_offset);
        std::memcpy(blocks[current_block].data + block_offset,
                   static_cast<const uint8_t*>(buffer) + bytes_written,
                   bytes_to_write);

        bytes_written += bytes_to_write;
        block_offset = (block_offset + bytes_to_write) % BLOCK_SIZE;
    }

    // Update version info
    VersionInfo new_version;
    new_version.version_number = fd_entry.inode->version_count + 1;
    new_version.block_index = new_first_block;
    new_version.size = fd_entry.current_position + bytes_written;
    new_version.timestamp = get_current_timestamp();
    
    // Update inode
    fd_entry.inode->version_history.push_back(new_version);
    fd_entry.inode->first_block = new_first_block;
    fd_entry.inode->size = fd_entry.current_position + bytes_written;
    fd_entry.inode->version_count++;
    fd_entry.current_position += bytes_written;

    std::cout << "Write operation completed:"
              << "\n  bytes written: " << bytes_written
              << "\n  new version: " << fd_entry.inode->version_count
              << "\n  new size: " << fd_entry.inode->size
              << std::endl;

    return bytes_written;
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
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!blocks[i].is_used) {
            blocks[i].is_used = true;
            blocks[i].next_block = 0;
            block_index = i;
            return true;
        }
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
    // This is a simplified version. In a real implementation, you would need to
    // track block references and free unreachable blocks.
    // For now, we'll just free blocks that are marked as unused.
    for (auto& block : blocks) {
        if (!block.is_used) {
            block.next_block = 0;
            std::memset(block.data, 0, BLOCK_SIZE);
        }
    }
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
    }

    // Initialize all blocks
    for (auto& block : blocks) {
        block.is_used = false;
        block.next_block = 0;
        std::memset(block.data, 0, BLOCK_SIZE);
    }
}

} // namespace cowfs 