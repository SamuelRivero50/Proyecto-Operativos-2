#include "cowfs.hpp"
#include "cowfs_utils.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <vector>

void print_file_status(const cowfs::FileStatus& status) {
    std::cout << "File Status:" << std::endl;
    std::cout << "  Is Open: " << (status.is_open ? "Yes" : "No") << std::endl;
    std::cout << "  Is Modified: " << (status.is_modified ? "Yes" : "No") << std::endl;
    std::cout << "  Current Size: " << status.current_size << " bytes" << std::endl;
    std::cout << "  Current Version: " << status.current_version << std::endl;
}

void save_and_print_metadata_json(cowfs::COWFileSystem& fs, const std::string& version_label) {
    // Create the metadata string
    std::stringstream json_output;
    json_output << "{\n";
    json_output << "  \"filesystem\": {\n";
    json_output << "    \"total_memory_usage\": " << fs.get_total_memory_usage() << ",\n";
    
    std::vector<std::string> files;
    fs.list_files(files);
    
    json_output << "    \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& filename = files[i];
        cowfs::fd_t fd = fs.open(filename, cowfs::FileMode::READ);
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

    // Print to console
    std::cout << "\nFile System Metadata (JSON format):\n" << json_output.str() << std::endl;

    // Save to file
    std::string filename = "metadata_" + version_label + ".json";
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << json_output.str();
        outfile.close();
        std::cout << "Metadata saved to " << filename << std::endl;
    } else {
        std::cerr << "Failed to save metadata to " << filename << std::endl;
    }
}

void read_and_print_file(cowfs::COWFileSystem& fs, cowfs::fd_t fd) {
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

void print_file_content(cowfs::COWFileSystem& fs, const std::string& filename) {
    cowfs::fd_t fd = fs.open(filename, cowfs::FileMode::READ);
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

void print_version_info(cowfs::COWFileSystem& fs, const std::string& filename) {
    cowfs::fd_t fd = fs.open(filename, cowfs::FileMode::READ);
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

void list_all_files(const cowfs::COWFileSystem& fs) {
    std::vector<std::string> files;
    if (fs.list_files(files)) {
        std::cout << "\n=== Files in the system ===" << std::endl;
        if (files.empty()) {
            std::cout << "No files found in the system." << std::endl;
        } else {
            for (const auto& filename : files) {
                std::cout << "\nFile: " << filename << std::endl;
                
                // Open file to get its details
                cowfs::fd_t fd = fs.open(filename, cowfs::FileMode::READ);
                if (fd >= 0) {
                    // Get file status
                    cowfs::FileStatus status = fs.get_file_status(fd);
                    print_file_status(status);
                    
                    // Get version history
                    auto versions = fs.get_version_history(fd);
                    std::cout << "Version History:" << std::endl;
                    for (const auto& version : versions) {
                        std::cout << "  Version " << version.version_number 
                                << " (Size: " << version.size 
                                << " bytes, Block: " << version.block_index 
                                << ", Time: " << version.timestamp << ")" << std::endl;
                    }
                    
                    // Close the file
                    fs.close(fd);
                }
            }
        }
        std::cout << "\nTotal Memory Usage: " << fs.get_total_memory_usage() << " bytes" << std::endl;
    }
}

int main() {
    // Delete existing disk files if they exist
    if (cowfs::COWFileSystem::delete_disk("disk.bin")) {
        std::cout << "Deleted existing disk.bin" << std::endl;
    }
    if (cowfs::COWFileSystem::delete_disk("cowfs.disk")) {
        std::cout << "Deleted existing cowfs.disk" << std::endl;
    }

    // Create a new file system
    cowfs::COWFileSystem fs("disk.bin", 1024 * 1024); // 1MB disk

    // Create a new file
    cowfs::fd_t fd = fs.create("test.txt");
    if (fd == -1) {
        std::cerr << "Failed to create file" << std::endl;
        return 1;
    }

    // Write some content
    const char* content = "Hello, World!";
    if (fs.write(fd, content, strlen(content)) == -1) {
        std::cerr << "Failed to write to file" << std::endl;
        return 1;
    }

    // Close the file
    fs.close(fd);

    // List all files
    std::vector<std::string> files;
    fs.list_files(files);
    std::cout << "\nFiles in the system:" << std::endl;
    for (const auto& file : files) {
        std::cout << "- " << file << std::endl;
    }

    // Open the file for reading
    fd = fs.open("test.txt", cowfs::FileMode::READ);
    if (fd == -1) {
        std::cerr << "Failed to open file for reading" << std::endl;
        return 1;
    }

    // Read and print content
    char buffer[256];
    ssize_t bytes_read = fs.read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << "\nFile content: " << buffer << std::endl;
    }

    // Close the file
    fs.close(fd);

    // Open the file for writing to append content
    fd = fs.open("test.txt", cowfs::FileMode::WRITE);
    if (fd == -1) {
        std::cerr << "Failed to open file for writing" << std::endl;
        return 1;
    }

    // Append new content
    const char* new_content = "\nThis is a new version!";
    if (fs.write(fd, new_content, strlen(new_content)) == -1) {
        std::cerr << "Failed to append to file" << std::endl;
        return 1;
    }

    // Close the file
    fs.close(fd);

    // List files again to show changes
    files.clear();
    fs.list_files(files);
    std::cout << "\nFiles in the system after append:" << std::endl;
    for (const auto& file : files) {
        std::cout << "- " << file << std::endl;
    }

    return 0;
} 