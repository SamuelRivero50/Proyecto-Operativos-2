#ifndef COWFS_HPP
#define COWFS_HPP

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <chrono>

namespace cowfs {

// Constants
constexpr size_t BLOCK_SIZE = 4096;
constexpr size_t MAX_FILENAME_LENGTH = 255;
constexpr size_t MAX_FILES = 1024;

// File descriptor type
using fd_t = int32_t;

// File mode flags
enum class FileMode {
    READ = 0x01,
    WRITE = 0x02,
    CREATE = 0x04
};

// File status flags
struct FileStatus {
    bool is_open;
    bool is_modified;
    size_t current_size;
    size_t current_version;
};

// Block structure
struct Block {
    uint8_t data[BLOCK_SIZE];
    size_t next_block;
    bool is_allocated;
    // New fields for delta storage
    bool is_delta;
    size_t base_block;     // Reference to original block
    size_t delta_start;    // Start position in base block
    size_t delta_length;   // Length of content to use from base block
};

// Version history structure
struct VersionInfo {
    size_t version_number;
    size_t block_index;
    size_t size;
    std::string timestamp;
};

// Inode structure
struct Inode {
    char filename[MAX_FILENAME_LENGTH];
    size_t first_block;
    size_t size;
    size_t version_count;
    bool is_used;
    std::vector<VersionInfo> version_history;  // Track version history
};

// File descriptor entry
struct FileDescriptor {
    Inode* inode;
    FileMode mode;
    size_t current_position;
    bool is_valid;
};

// Main COW file system class
class COWFileSystem {
public:
    // Constructor sin parámetros para crear un nuevo sistema de archivos
    COWFileSystem();
    
    // Constructor con parámetros para cargar un sistema existente
    COWFileSystem(const std::string& disk_path, size_t disk_size);
    
    ~COWFileSystem();

    // Static function to delete a disk file
    static bool delete_disk(const std::string& disk_path);

    // Formatear el sistema de archivos
    bool format();

    // Core file operations
    fd_t create(const std::string& filename);
    fd_t open(const std::string& filename, FileMode mode) const;
    ssize_t read(fd_t fd, void* buffer, size_t size);
    ssize_t write(fd_t fd, const void* buffer, size_t size);
    int close(fd_t fd) const;

    // Version management
    size_t get_version_count(fd_t fd) const;
    bool revert_to_version(fd_t fd, size_t version);
    std::vector<VersionInfo> get_version_history(fd_t fd) const;

    // File system operations
    bool list_files(std::vector<std::string>& files) const;
    size_t get_file_size(fd_t fd) const;
    FileStatus get_file_status(fd_t fd) const;

    // Memory management
    size_t get_total_memory_usage() const;
    void garbage_collect();

    // Metadata operations
    void print_metadata_json() const;
    bool save_metadata_json(const std::string& version_label) const;

private:
    // Internal helper functions
    bool initialize_disk();
    const Inode* find_inode(const std::string& filename) const;
    fd_t allocate_file_descriptor();
    void free_file_descriptor(fd_t fd);
    bool allocate_block(size_t& block_index);
    void free_block(size_t block_index);
    bool copy_block(size_t source_block, size_t& dest_block);

    // File descriptor management
    std::vector<FileDescriptor> file_descriptors;
    std::vector<Inode> inodes;
    std::vector<Block> blocks;
    std::string disk_path;
    size_t disk_size;
    size_t total_blocks;
    bool is_formatted;

    void init_file_system();
};

} // namespace cowfs

#endif // COWFS_HPP 