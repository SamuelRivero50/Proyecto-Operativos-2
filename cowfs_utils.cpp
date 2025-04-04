#include "cowfs_utils.hpp"
#include <iostream>
#include <cstring>

namespace cowfs {

void FileSystemUtils::print_file_status(const FileStatus& status) {
    std::cout << "File Status:" << std::endl;
    std::cout << "  Is Open: " << (status.is_open ? "Yes" : "No") << std::endl;
    std::cout << "  Is Modified: " << (status.is_modified ? "Yes" : "No") << std::endl;
    std::cout << "  Current Size: " << status.current_size << " bytes" << std::endl;
    std::cout << "  Current Version: " << status.current_version << std::endl;
}

void FileSystemUtils::read_and_print_file(COWFileSystem& fs, fd_t fd) {
    char buffer[256] = {0};
    ssize_t read = fs.read(fd, buffer, sizeof(buffer) - 1);
    if (read < 0) {
        std::cerr << "Failed to read from file" << std::endl;
        return;
    }
    buffer[read] = '\0';
    std::cout << "Read from file: " << buffer << std::endl;
    print_file_status(fs.get_file_status(fd));
}

void FileSystemUtils::print_file_content(COWFileSystem& fs, const std::string& filename) {
    fd_t fd = fs.open(filename, FileMode::READ);
    if (fd < 0) {
        std::cerr << "Failed to open file for reading" << std::endl;
        return;
    }

    char buffer[4096];
    ssize_t bytes_read;
    std::cout << "File content: ";
    while ((bytes_read = fs.read(fd, buffer, sizeof(buffer))) > 0) {
        std::cout.write(buffer, bytes_read);
    }
    std::cout << std::endl;
    fs.close(fd);
}

void FileSystemUtils::print_version_info(COWFileSystem& fs, const std::string& filename) {
    fd_t fd = fs.open(filename, FileMode::READ);
    if (fd < 0) {
        std::cerr << "Failed to open file for reading" << std::endl;
        return;
    }

    auto versions = fs.get_version_history(fd);
    std::cout << "\nVersion history for " << filename << ":" << std::endl;
    for (const auto& version : versions) {
        std::cout << "Version " << version.version_number 
                  << " (size: " << version.size 
                  << ", timestamp: " << version.timestamp 
                  << ", block: " << version.block_index << ")" << std::endl;
    }
    fs.close(fd);
}

void FileSystemUtils::list_all_files(const COWFileSystem& fs) {
    std::vector<std::string> files;
    if (fs.list_files(files)) {
        std::cout << "\n=== Files in the system ===" << std::endl;
        if (files.empty()) {
            std::cout << "No files found in the system." << std::endl;
        } else {
            for (const auto& filename : files) {
                std::cout << "\nFile: " << filename << std::endl;
                
                fd_t fd = fs.open(filename, FileMode::READ);
                if (fd >= 0) {
                    FileStatus status = fs.get_file_status(fd);
                    print_file_status(status);
                    
                    auto versions = fs.get_version_history(fd);
                    std::cout << "Version History:" << std::endl;
                    for (const auto& version : versions) {
                        std::cout << "  Version " << version.version_number 
                                << " (Size: " << version.size 
                                << " bytes, Block: " << version.block_index 
                                << ", Time: " << version.timestamp << ")" << std::endl;
                    }
                    
                    fs.close(fd);
                }
            }
        }
        std::cout << "\nTotal Memory Usage: " << fs.get_total_memory_usage() << " bytes" << std::endl;
    }
}

void FileSystemUtils::print_metadata_json(const COWFileSystem& fs) {
    std::stringstream json_output;
    json_output << "{\n";
    json_output << "  \"filesystem\": {\n";
    json_output << "    \"total_memory_usage\": " << fs.get_total_memory_usage() << ",\n";
    
    std::vector<std::string> files;
    fs.list_files(files);
    
    json_output << "    \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& filename = files[i];
        fd_t fd = fs.open(filename, FileMode::READ);
        if (fd >= 0) {
            auto status = fs.get_file_status(fd);
            json_output << "      {\n";
            json_output << "        \"name\": \"" << filename << "\",\n";
            json_output << "        \"size\": " << status.current_size << ",\n";
            json_output << "        \"version_count\": " << status.current_version << ",\n";
            json_output << "        \"is_open\": " << (status.is_open ? "true" : "false") << ",\n";
            
            json_output << "        \"version_history\": [\n";
            auto version_history = fs.get_version_history(fd);
            for (size_t j = 0; j < version_history.size(); ++j) {
                const auto& version = version_history[j];
                json_output << "          {\n";
                json_output << "            \"version_number\": " << version.version_number << ",\n";
                json_output << "            \"block_index\": " << version.block_index << ",\n";
                json_output << "            \"size\": " << version.size << ",\n";
                json_output << "            \"timestamp\": \"" << version.timestamp << "\"\n";
                json_output << "          }" << (j < version_history.size() - 1 ? "," : "") << "\n";
            }
            json_output << "        ]\n";
            
            json_output << "      }" << (i < files.size() - 1 ? "," : "") << "\n";
            fs.close(fd);
        }
    }
    json_output << "    ]\n";
    json_output << "  }\n";
    json_output << "}\n";

    std::cout << "\nFile System Metadata (JSON format):\n" << json_output.str() << std::endl;
}

bool FileSystemUtils::save_metadata_json(const COWFileSystem& fs, const std::string& version_label) {
    std::stringstream json_output;
    json_output << "{\n";
    json_output << "  \"filesystem\": {\n";
    json_output << "    \"total_memory_usage\": " << fs.get_total_memory_usage() << ",\n";
    
    std::vector<std::string> files;
    fs.list_files(files);
    
    json_output << "    \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& filename = files[i];
        fd_t fd = fs.open(filename, FileMode::READ);
        if (fd >= 0) {
            auto status = fs.get_file_status(fd);
            json_output << "      {\n";
            json_output << "        \"name\": \"" << filename << "\",\n";
            json_output << "        \"size\": " << status.current_size << ",\n";
            json_output << "        \"version_count\": " << status.current_version << ",\n";
            json_output << "        \"is_open\": " << (status.is_open ? "true" : "false") << ",\n";
            
            json_output << "        \"version_history\": [\n";
            auto version_history = fs.get_version_history(fd);
            for (size_t j = 0; j < version_history.size(); ++j) {
                const auto& version = version_history[j];
                json_output << "          {\n";
                json_output << "            \"version_number\": " << version.version_number << ",\n";
                json_output << "            \"block_index\": " << version.block_index << ",\n";
                json_output << "            \"size\": " << version.size << ",\n";
                json_output << "            \"timestamp\": \"" << version.timestamp << "\"\n";
                json_output << "          }" << (j < version_history.size() - 1 ? "," : "") << "\n";
            }
            json_output << "        ]\n";
            
            json_output << "      }" << (i < files.size() - 1 ? "," : "") << "\n";
            fs.close(fd);
        }
    }
    json_output << "    ]\n";
    json_output << "  }\n";
    json_output << "}\n";

    std::string filename = "metadata_" + version_label + ".json";
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << json_output.str();
        outfile.close();
        std::cout << "Metadata saved to " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Failed to save metadata to " << filename << std::endl;
        return false;
    }
}

} // namespace cowfs 