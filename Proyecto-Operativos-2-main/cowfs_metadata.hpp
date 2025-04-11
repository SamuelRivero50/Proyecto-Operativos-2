#ifndef COWFS_METADATA_HPP
#define COWFS_METADATA_HPP

#include "cowfs.hpp"
#include <string>

namespace cowfs {

class MetadataManager {
public:
    // Save metadata to a JSON file and print to console
    static bool save_and_print_metadata(COWFileSystem& fs, const std::string& version_label);
    
    // Only print metadata to console
    static void print_metadata(COWFileSystem& fs);
    
    // Only save metadata to file
    static bool save_metadata(COWFileSystem& fs, const std::string& version_label);

private:
    // Helper function to generate JSON string
    static std::string generate_metadata_json(COWFileSystem& fs);
};

} // namespace cowfs

#endif // COWFS_METADATA_HPP 