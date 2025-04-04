#ifndef COWFS_HPP
#define COWFS_HPP

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <cstring>

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
    bool is_used;
    size_t ref_count;       // Contador de referencias para bloques compartidos
};

// Version history structure
struct VersionInfo {
    size_t version_number;
    size_t block_index;
    size_t size;
    std::string timestamp;
    size_t delta_start;      // Índice donde comienzan los cambios
    size_t delta_size;       // Tamaño de los cambios
    size_t prev_version;     // Referencia a la versión anterior
};

// Inode structure
struct Inode {
    char filename[MAX_FILENAME_LENGTH];
    size_t first_block;
    size_t size;
    size_t version_count;
    bool is_used;
    std::vector<VersionInfo> version_history;
    std::vector<size_t> shared_blocks;  // Bloques compartidos entre versiones
};

// Estructura para manejar bloques libres
struct FreeBlockInfo {
    size_t start_block;
    size_t block_count;
    FreeBlockInfo* next;
};

// Main COW file system class
class COWFileSystem {
public:
    COWFileSystem(const std::string& disk_path, size_t disk_size);
    ~COWFileSystem();

    // Core file operations
    fd_t create(const std::string& filename);
    fd_t open(const std::string& filename, FileMode mode);
    ssize_t read(fd_t fd, void* buffer, size_t size);
    ssize_t write(fd_t fd, const void* buffer, size_t size);
    int close(fd_t fd);

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

private:
    // Internal helper functions
    bool initialize_disk();
    Inode* find_inode(const std::string& filename);
    fd_t allocate_file_descriptor();
    void free_file_descriptor(fd_t fd);
    bool allocate_block(size_t& block_index);
    void free_block(size_t block_index);
    bool copy_block(size_t source_block, size_t& dest_block);

    // File descriptor management
    struct FileDescriptor {
        Inode* inode;
        FileMode mode;
        size_t current_position;
        bool is_valid;
    };

    std::vector<FileDescriptor> file_descriptors;
    std::vector<Inode> inodes;
    std::vector<Block> blocks;
    std::string disk_path;
    size_t disk_size;
    size_t total_blocks;

    // Lista enlazada de bloques libres
    FreeBlockInfo* free_blocks_list;
    
    // Nuevos métodos privados para gestión de memoria
    bool merge_free_blocks();
    bool split_free_block(FreeBlockInfo* block, size_t size_needed);
    void add_to_free_list(size_t start, size_t count);
    FreeBlockInfo* find_best_fit(size_t blocks_needed);

    void init_file_system();

    // Nuevos métodos para manejo de versiones incrementales
    bool find_delta(const void* old_data, const void* new_data, 
                   size_t old_size, size_t new_size,
                   size_t& delta_start, size_t& delta_size);
    bool write_delta_blocks(const void* buffer, size_t size, 
                          size_t delta_start, size_t& first_block);
    bool read_version_data(size_t version, fd_t fd, void* buffer, size_t& size);
    void increment_block_refs(size_t block_index);
    void decrement_block_refs(size_t block_index);
};

} // namespace cowfs

#endif // COWFS_HPP 