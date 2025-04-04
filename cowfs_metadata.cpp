#include "cowfs_metadata.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace cowfs {

bool MetadataManager::create_directory(const std::string& path) {
    #ifdef _WIN32
        return _mkdir(path.c_str()) == 0;
    #else
        return mkdir(path.c_str(), 0755) == 0;
    #endif
}

bool MetadataManager::directory_exists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

std::string MetadataManager::build_path(const std::string& dir, const std::string& filename) {
    if (dir.empty()) return filename;
    if (dir.back() == '/') return dir + filename;
    return dir + "/" + filename;
}

std::string MetadataManager::generate_file_json(const std::string& filename, COWFileSystem& fs) {
    std::stringstream json_output;
    fd_t fd = fs.open(filename, FileMode::READ);

    if (fd >= 0) {
        auto status = fs.get_file_status(fd);
        json_output << "{\n";
        json_output << "  \"file\": {\n";
        json_output << "    \"name\": \"" << filename << "\",\n";
        json_output << "    \"size\": " << status.current_size << ",\n";
        json_output << "    \"version_count\": " << status.current_version << ",\n";
        json_output << "    \"is_open\": " << (status.is_open ? "true" : "false") << ",\n";

        json_output << "    \"version_history\": [\n";
        auto version_history = fs.get_version_history(fd);
        for (size_t j = 0; j < version_history.size(); ++j) {
            const auto& version = version_history[j];
            json_output << "      {\n";
            json_output << "        \"version_number\": " << version.version_number << ",\n";
            json_output << "        \"block_index\": " << version.block_index << ",\n";
            json_output << "        \"size\": " << version.size << ",\n";
            json_output << "        \"timestamp\": \"" << version.timestamp << "\"\n";
            json_output << "      }" << (j < version_history.size() - 1 ? "," : "") << "\n";
        }
        json_output << "    ]\n";
        json_output << "  }\n";
        json_output << "}";

        fs.close(fd);
    }

    return json_output.str();
}

std::string MetadataManager::generate_metadata_json(COWFileSystem& fs) {
    std::stringstream json_output;
    json_output << "{\n";
    json_output << "  \"filesystem\": {\n";
    json_output << "    \"total_memory_usage\": " << fs.get_total_memory_usage() << ",\n";

    std::vector<std::string> files;
    fs.list_files(files);

    json_output << "    \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        json_output << "      \"" << files[i] << "\"" << (i < files.size() - 1 ? "," : "") << "\n";
    }
    json_output << "    ]\n";
    json_output << "  }\n";
    json_output << "}";

    return json_output.str();
}

bool MetadataManager::save_metadata_files(COWFileSystem& fs, const std::string& output_dir,
                                        const std::string& version_label) {
    // Crear directorio si no existe
    if (!directory_exists(output_dir)) {
        if (!create_directory(output_dir)) {
            std::cerr << "Failed to create directory: " << output_dir
                      << " - " << strerror(errno) << std::endl;
            return false;
        }
    }

    // Guardar metadatos generales
    std::string general_metadata = generate_metadata_json(fs);
    std::string general_filename = build_path(output_dir, "metadata_" + version_label + ".json");

    std::ofstream general_out(general_filename);
    if (!general_out.is_open()) {
        std::cerr << "Failed to open general metadata file: " << general_filename << std::endl;
        return false;
    }
    general_out << general_metadata << std::endl;
    general_out.close();

    // Guardar metadatos individuales de archivos
    std::vector<std::string> files;
    fs.list_files(files);

    for (const auto& filename : files) {
        std::string file_metadata = generate_file_json(filename, fs);
        std::string file_filename = build_path(output_dir, filename + "_metadata_" + version_label + ".json");

        std::ofstream file_out(file_filename);
        if (!file_out.is_open()) {
            std::cerr << "Failed to open file metadata file: " << file_filename << std::endl;
            continue;
        }
        file_out << file_metadata << std::endl;
        file_out.close();
    }

    return true;
}

bool MetadataManager::save_and_print_metadata(COWFileSystem& fs, const std::string& version_label) {
    // Primero guardar todos los archivos de metadatos
    bool success = save_metadata_files(fs, "metadata", version_label);

    // Luego imprimir metadatos generales a consola
    std::string json_str = generate_metadata_json(fs);
    std::cout << "\nFile System Metadata (JSON format):\n" << json_str << std::endl;

    return success;
}

void MetadataManager::print_metadata(COWFileSystem& fs) {
    std::string json_str = generate_metadata_json(fs);
    std::cout << "\nFile System Metadata (JSON format):\n" << json_str << std::endl;
}

bool MetadataManager::save_metadata(COWFileSystem& fs, const std::string& version_label) {
    return save_metadata_files(fs, "metadata", version_label);
}

} // namespace cowfs