#include "cowfs.hpp"
#include "cowfs_metadata.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <fstream>

void print_file_status(const cowfs::COWFileSystem& fs, cowfs::fd_t fd) {
    auto status = fs.get_file_status(fd);
    std::cout << "File status:\n"
              << "  Is open: " << (status.is_open ? "Yes" : "No") << "\n"
              << "  Is modified: " << (status.is_modified ? "Yes" : "No") << "\n"
              << "  Current size: " << status.current_size << " bytes\n"
              << "  Current version: " << status.current_version << "\n"
              << "  Total memory usage: " << fs.get_total_memory_usage() << " bytes\n"
              << std::endl;
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
    print_file_status(fs, fd);
}

int main() {
    try {
        // Create a COW file system with 1MB of space
        cowfs::COWFileSystem fs("cowfs.disk", 1024 * 1024);

        // Try to open the file first, if it exists
        std::cout << "Trying to open 'test.txt'..." << std::endl;
        cowfs::fd_t fd = fs.open("test.txt", cowfs::FileMode::WRITE);
        
        // If file doesn't exist, create it
        if (fd < 0) {
            std::cout << "File doesn't exist, creating 'test.txt'..." << std::endl;
            fd = fs.create("test.txt");
            if (fd < 0) {
                std::cerr << "Failed to create file" << std::endl;
                return 1;
            }
        }

        // Write first version
        std::cout << "\nWriting first version..." << std::endl;
        const char* data1 = "Hello, COW File System!";
        ssize_t written = fs.write(fd, data1, strlen(data1));
        if (written < 0) {
            std::cerr << "Failed to write to file" << std::endl;
            return 1;
        }
        std::cout << "Wrote " << written << " bytes to file" << std::endl;
        print_file_status(fs, fd);
        cowfs::MetadataManager::save_and_print_metadata(fs, "v1");

        // Close the file
        fs.close(fd);

        // Reopen and read first version
        std::cout << "\nReading first version..." << std::endl;
        fd = fs.open("test.txt", cowfs::FileMode::READ);
        if (fd < 0) {
            std::cerr << "Failed to open file" << std::endl;
            return 1;
        }
        read_and_print_file(fs, fd);
        fs.close(fd);

        // Open in write mode and append more content
        std::cout << "\nWriting second version..." << std::endl;
        fd = fs.open("test.txt", cowfs::FileMode::WRITE);
        if (fd < 0) {
            std::cerr << "Failed to open file" << std::endl;
            return 1;
        }
        const char* data2 = " This is the second version!";
        written = fs.write(fd, data2, strlen(data2));
        if (written < 0) {
            std::cerr << "Failed to write to file" << std::endl;
            return 1;
        }
        std::cout << "Wrote " << written << " bytes to file" << std::endl;
        print_file_status(fs, fd);
        cowfs::MetadataManager::save_and_print_metadata(fs, "v2");
        fs.close(fd);

        // Read the latest version
        std::cout << "\nReading latest version..." << std::endl;
        fd = fs.open("test.txt", cowfs::FileMode::READ);
        if (fd < 0) {
            std::cerr << "Failed to open file" << std::endl;
            return 1;
        }
        read_and_print_file(fs, fd);
        fs.close(fd);

        // Show final metadata
        cowfs::MetadataManager::save_and_print_metadata(fs, "final");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 