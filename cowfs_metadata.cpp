#include "cowfs_metadata.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

namespace cowfs {

std::string MetadataManager::get_current_timestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string MetadataManager::generate_metadata_json(COWFileSystem& fs) {
    std::stringstream json;
    json << std::boolalpha;
    json << "{\n";
    json << "  \"filesystem\": {\n";
    json << "    \"total_memory_usage\": " << fs.get_total_memory_usage() << ",\n";

    std::vector<std::string> files;
    fs.list_files(files);

    json << "    \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& filename = files[i];
        fd_t fd = fs.open(filename, FileMode::READ);
        if (fd >= 0) {
            auto status = fs.get_file_status(fd);
            json << "      {\n";
            json << "        \"name\": \"" << filename << "\",\n";
            json << "        \"size\": " << status.current_size << ",\n";
            json << "        \"version_count\": " << status.current_version << ",\n";
            json << "        \"is_open\": " << (status.is_open ? "true" : "false") << ",\n";

            json << "        \"version_history\": [\n";
            auto version_history = fs.get_version_history(fd);
            for (size_t j = 0; j < version_history.size(); ++j) {
                const auto& version = version_history[j];
                json << "          {\n";
                json << "            \"version_number\": " << version.version_number << ",\n";
                json << "            \"block_index\": " << version.block_index << ",\n";
                json << "            \"size\": " << version.size << ",\n";
                json << "            \"timestamp\": \"" << (version.timestamp.empty() ? get_current_timestamp() : version.timestamp) << "\"\n";
                json << "          }" << (j < version_history.size() - 1 ? "," : "") << "\n";
            }
            json << "        ]\n";

            json << "      }" << (i < files.size() - 1 ? "," : "") << "\n";
            fs.close(fd);
        }
    }
    json << "    ]\n";
    json << "  }\n";
    json << "}";

    return json.str();
}

bool MetadataManager::save_and_print_metadata(COWFileSystem& fs, const std::string& version_label) {
    std::string json_str = generate_metadata_json(fs);

    // Print to console
    std::cout << "\nFile System Metadata (JSON format):\n" << json_str << std::endl;

    // Save to file
    std::string filename = "metadata_" + version_label + ".json";
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << json_str;
        outfile.close();
        std::cout << "Metadata saved to " << filename << std::endl;
        return true;
    } else {
        std::cerr << "Failed to save metadata to " << filename << std::endl;
        return false;
    }
}

void MetadataManager::print_metadata(COWFileSystem& fs) {
    std::cout << "\nFile System Metadata (JSON format):\n" << generate_metadata_json(fs) << std::endl;
}

bool MetadataManager::save_metadata(COWFileSystem& fs, const std::string& version_label) {
    std::string json_str = generate_metadata_json(fs);
    std::string filename = "metadata_" + version_label + ".json";
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << json_str;
        outfile.close();
        return true;
    }
    return false;
}

} // namespace cowfs