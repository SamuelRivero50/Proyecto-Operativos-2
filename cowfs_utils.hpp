#ifndef COWFS_UTILS_HPP
#define COWFS_UTILS_HPP

#include "cowfs.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>

namespace cowfs {

// Utility functions for file system operations
class FileSystemUtils {
public:
    // File status printing
    static void print_file_status(const FileStatus& status);
    
    // File content operations
    static void read_and_print_file(COWFileSystem& fs, fd_t fd);
    static void print_file_content(COWFileSystem& fs, const std::string& filename);
    
    // Version management
    static void print_version_info(COWFileSystem& fs, const std::string& filename);
    
    // File listing
    static void list_all_files(const COWFileSystem& fs);
    
    // Metadata operations
    static void print_metadata_json(const COWFileSystem& fs);
    static bool save_metadata_json(const COWFileSystem& fs, const std::string& version_label);
};

} // namespace cowfs

#endif // COWFS_UTILS_HPP 