#ifndef COWFS_METADATA_HPP
#define COWFS_METADATA_HPP

#include "cowfs.hpp"
#include <string>
#include <vector>

namespace cowfs {

    class MetadataManager {
    public:
        static bool save_metadata_files(COWFileSystem& fs, const std::string& output_dir,
                                      const std::string& version_label);
        static bool save_and_print_metadata(COWFileSystem& fs, const std::string& version_label);
        static void print_metadata(COWFileSystem& fs);
        static bool save_metadata(COWFileSystem& fs, const std::string& version_label);

    private:
        static std::string generate_file_json(const std::string& filename, COWFileSystem& fs);
        static std::string generate_metadata_json(COWFileSystem& fs);
        static bool create_directory(const std::string& path);
        static bool directory_exists(const std::string& path);
        static std::string build_path(const std::string& dir, const std::string& filename);
    };

} // namespace cowfs

#endif // COWFS_METADATA_HPP