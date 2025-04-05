#include "cowfs.hpp"
#include "cowfs_metadata.hpp"
#include <iostream>
#include <string>
#include <cstring>

void print_file_status(const cowfs::COWFileSystem& fs, cowfs::fd_t fd) {
    auto status = fs.get_file_status(fd);
    std::cout << "File status:\n";
    std::cout << "  Is open: " << (status.is_open ? "Yes" : "No") << "\n";
    std::cout << "  Is modified: " << (status.is_modified ? "Yes" : "No") << "\n";
    std::cout << "  Current size: " << status.current_size << " bytes\n";
    std::cout << "  Current version: " << status.current_version << "\n";
    std::cout << "  Total memory usage: " << fs.get_total_memory_usage() << " bytes\n\n";
}

void read_and_print_file(cowfs::COWFileSystem& fs, cowfs::fd_t fd) {
    char buffer[256] = {0};
    ssize_t bytes_read = fs.read(fd, buffer, sizeof(buffer)-1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << "Read from file: " << buffer << std::endl;
    } else {
        std::cout << "Read from file:" << std::endl;
    }
    print_file_status(fs, fd);
}

int main() {
    try {
        // Initialize file system with 1MB
        const std::string disk_path = "cowfs.disk";
        const size_t disk_size = 1024 * 1024;
        cowfs::COWFileSystem fs(disk_path, disk_size);

        std::cout << "Initializing file system with size: " << disk_size << " bytes\n";
        std::cout << "Total blocks: " << (disk_size / 4096) << "\n";
        std::cout << "File system initialized with:\n";
        std::cout << "  Max files: 1024\n";
        std::cout << "  Block size: 4096 bytes\n";

        // Test file operations
        const std::string filename = "test.txt";

        // Try to open non-existent file
        std::cout << "\nTrying to open '" << filename << "'...\n";
        cowfs::fd_t fd = fs.open(filename, cowfs::FileMode::READ);
        if (fd < 0) {
            std::cout << "File not found in open\n";
            std::cout << "File doesn't exist, creating '" << filename << "'...\n";
            fd = fs.create(filename); // Usar create() en lugar de open() para WRITE
            if (fd >= 0) {
                std::cout << "Successfully created file with fd: " << fd << "\n\n";
            } else {
                std::cerr << "Failed to create file\n";
                return 1;
            }
        } else {
            // Si el archivo existe, cerramos y reabrimos en modo WRITE
            fs.close(fd);
            fd = fs.open(filename, cowfs::FileMode::WRITE);
        }

        // Write first version
        std::cout << "Writing first version...\n";
        const std::string content1 = "This is the first version";
        ssize_t written = fs.write(fd, content1.c_str(), content1.size());
        if (written < 0) {
            std::cerr << "Failed to write to file\n";
            return 1;
        }
        std::cout << "Wrote " << written << " bytes to file\n";
        print_file_status(fs, fd);

        // Save metadata v1
        cowfs::MetadataManager::save_and_print_metadata(fs, "v1");

        // Read first version
        std::cout << "\nReading first version...\n";
        fs.close(fd);
        fd = fs.open(filename, cowfs::FileMode::READ);
        read_and_print_file(fs, fd);

        // Write second version
        std::cout << "\nWriting second version...\n";
        fs.close(fd);
        fd = fs.open(filename, cowfs::FileMode::WRITE);
        const std::string content2 = "This is the second version!";
        written = fs.write(fd, content2.c_str(), content2.size());
        if (written < 0) {
            std::cerr << "Failed to write to file\n";
            return 1;
        }
        std::cout << "Wrote " << written << " bytes to file\n";
        print_file_status(fs, fd);

        // Save metadata v2
        cowfs::MetadataManager::save_and_print_metadata(fs, "v2");

        // Read latest version
        std::cout << "\nReading latest version...\n";
        fs.close(fd);
        fd = fs.open(filename, cowfs::FileMode::READ);
        read_and_print_file(fs, fd);

        // Save final metadata
        cowfs::MetadataManager::save_and_print_metadata(fs, "final");

        fs.close(fd);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}