#include "cowfs_metadata.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>

namespace cowfs {

std::string escape_json(const std::string &s) {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        switch (*c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if ('\x00' <= *c && *c <= '\x1f') {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
            } else {
                o << *c;
            }
        }
    }
    return o.str();
}

std::string get_current_timestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string MetadataManager::generate_metadata_json(COWFileSystem& fs) {
    try {
        std::stringstream json_output;
        json_output << "{\n";
        json_output << "  \"metadata\": {\n";
        json_output << "    \"timestamp\": \"" << get_current_timestamp() << "\",\n";
        json_output << "    \"filesystem\": {\n";
        json_output << "      \"total_memory_usage\": " << fs.get_total_memory_usage() << ",\n";
        
        std::vector<std::string> files;
        fs.list_files(files);
        
        json_output << "      \"files\": [\n";
        for (size_t i = 0; i < files.size(); ++i) {
            const auto& filename = files[i];
            fd_t fd = fs.open(filename, FileMode::READ);
            if (fd >= 0) {
                auto status = fs.get_file_status(fd);
                json_output << "        {\n";
                json_output << "          \"name\": \"" << escape_json(filename) << "\",\n";
                json_output << "          \"size\": " << status.current_size << ",\n";
                json_output << "          \"version_count\": " << status.current_version << ",\n";
                json_output << "          \"is_open\": " << (status.is_open ? "true" : "false") << ",\n";
                
                json_output << "          \"version_history\": [\n";
                auto version_history = fs.get_version_history(fd);
                for (size_t j = 0; j < version_history.size(); ++j) {
                    const auto& version = version_history[j];
                    json_output << "            {\n";
                    json_output << "              \"version_number\": " << version.version_number << ",\n";
                    json_output << "              \"block_index\": " << version.block_index << ",\n";
                    json_output << "              \"size\": " << version.size << ",\n";
                    json_output << "              \"timestamp\": \"" << escape_json(version.timestamp) << "\"\n";
                    json_output << "            }" << (j < version_history.size() - 1 ? "," : "") << "\n";
                }
                json_output << "          ]\n";
                
                json_output << "        }" << (i < files.size() - 1 ? "," : "") << "\n";
                fs.close(fd);
            }
        }
        json_output << "      ]\n";
        json_output << "    }\n";
        json_output << "  }\n";
        json_output << "}";
        
        return json_output.str();
    } catch (const std::exception& e) {
        std::cerr << "Error generating metadata JSON: " << e.what() << std::endl;
        return "{}";
    }
}

bool MetadataManager::save_and_print_metadata(COWFileSystem& fs, const std::string& version_label) {
    try {
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
    } catch (const std::exception& e) {
        std::cerr << "Error saving metadata: " << e.what() << std::endl;
        return false;
    }
}

void MetadataManager::print_metadata(COWFileSystem& fs) {
    try {
        std::string json_str = generate_metadata_json(fs);
        std::cout << "\nFile System Metadata (JSON format):\n" << json_str << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error printing metadata: " << e.what() << std::endl;
    }
}

bool MetadataManager::save_metadata(COWFileSystem& fs, const std::string& version_label) {
    try {
        std::string json_str = generate_metadata_json(fs);
        std::string filename = "metadata_" + version_label + ".json";
        std::ofstream outfile(filename);
        if (outfile.is_open()) {
            outfile << json_str << std::endl;
            outfile.close();
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error saving metadata: " << e.what() << std::endl;
        return false;
    }
}

} // namespace cowfs 