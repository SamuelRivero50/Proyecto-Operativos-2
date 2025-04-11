#include "cowfs_metadata.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace cowfs {

std::string MetadataManager::generate_metadata_json(COWFileSystem& fs) {
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
    json_output << "}";
    
    return json_output.str();
}

bool MetadataManager::save_and_print_metadata(COWFileSystem& fs, const std::string& version_label) {
    std::string json_str = generate_metadata_json(fs);
    
    // Print to console
    std::cout << "\nFile System Metadata (JSON format):\n" << json_str << std::endl;

    // Save to file
    std::string filename = "metadata_" + version_label + ".json";
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << json_str << std::endl;
        outfile.close();
        std::cout << "Metadata saved to " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Failed to save metadata to " << filename << std::endl;
        return false;
    }
}

void MetadataManager::print_metadata(COWFileSystem& fs) {
    std::string json_str = generate_metadata_json(fs);
    std::cout << "\nFile System Metadata (JSON format):\n" << json_str << std::endl;
}

bool MetadataManager::save_metadata(COWFileSystem& fs, const std::string& version_label) {
    std::string json_str = generate_metadata_json(fs);
    std::string filename = "metadata_" + version_label + ".json";
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << json_str << std::endl;
        outfile.close();
        return true;
    }
    return false;
}

} // namespace cowfs 