#ifndef COWFS_HPP
#define COWFS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <iostream> // Asegúrate de incluir esto

namespace cowfs {

// Constantes
constexpr size_t BLOCK_SIZE = 4096;
constexpr size_t MAX_FILENAME_LENGTH = 255;
constexpr size_t MAX_FILES = 1024;
using fd_t = int32_t; // Mueve esto aquí
constexpr fd_t INVALID_FD = -1; // Mueve esto aquí

// Modos de archivo
enum class FileMode {
    READ = 0x01,
    WRITE = 0x02,
    CREATE = 0x04
};

// Estado del archivo
struct FileStatus {
    bool is_open;
    bool is_modified;
    size_t current_size;
    size_t current_version;
};

// Estructura de bloque
struct Block {
    uint8_t data[BLOCK_SIZE];
    size_t next_block;
    bool is_used;

    Block() : next_block(0), is_used(false) {
        memset(data, 0, BLOCK_SIZE);
    }
};

// Información de versión
struct VersionInfo {
    size_t version_number;
    size_t block_index;
    size_t size;
    std::string timestamp;
};

// Estructura inodo
struct Inode {
    char filename[MAX_FILENAME_LENGTH];
    size_t first_block;
    size_t size;
    size_t version_count;
    bool is_used;
    std::vector<VersionInfo> version_history;

    Inode() : first_block(0), size(0), version_count(0), is_used(false) {
        memset(filename, 0, MAX_FILENAME_LENGTH);
    }
};

// Clase principal del sistema de archivos COW
class COWFileSystem {
public:
    COWFileSystem(const std::string& disk_path, size_t disk_size);
    ~COWFileSystem();

    // Operaciones básicas
    fd_t create(const std::string& filename);
    fd_t open(const std::string& filename, FileMode mode);
    ssize_t read(fd_t fd, void* buffer, size_t size);
    ssize_t write(fd_t fd, const void* buffer, size_t size);
    int close(fd_t fd);
    bool remove(const std::string& filename);

    // Gestión de versiones
    size_t get_version_count(fd_t fd) const;
    bool revert_to_version(fd_t fd, size_t version);
    std::vector<VersionInfo> get_version_history(fd_t fd) const;

    // Operaciones del sistema de archivos
    bool list_files(std::vector<std::string>& files) const;
    size_t get_file_size(fd_t fd) const;
    FileStatus get_file_status(fd_t fd) const;

    // Gestión de memoria
    size_t get_total_memory_usage() const;
    void garbage_collect();

    // Función de ayuda para timestamp
    static std::string get_current_timestamp() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

private:
    // Funciones internas
    bool initialize_disk();
    Inode* find_inode(const std::string& filename);
    fd_t allocate_file_descriptor();
    void free_file_descriptor(fd_t fd);
    bool allocate_block(size_t& block_index);
    void free_block(size_t block_index);
    bool copy_block(size_t source_block, size_t& dest_block);
    void free_block_chain(size_t block_index);
    void save_to_disk();
    void init_file_system();

    // Descriptor de archivo
    struct FileDescriptor {
        Inode* inode;
        FileMode mode;
        size_t current_position;
        bool is_valid;

        FileDescriptor() : inode(nullptr), mode(FileMode::READ),
                         current_position(0), is_valid(false) {}
    };

    std::vector<FileDescriptor> file_descriptors;
    std::vector<Inode> inodes;
    std::vector<Block> blocks;
    std::unordered_map<std::string, size_t> filename_to_inode;
    std::vector<size_t> free_blocks;
    std::vector<fd_t> free_fds;
    std::string disk_path;
    size_t disk_size;
    size_t total_blocks;
};

} // namespace cowfs

#endif // COWFS_HPP