#include <iostream>
#include <fstream>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

const int BLOCK_SIZE = 4096;
const std::string DISK_NAME = "filesystem.img";
const std::string META_FILE = "metadata.json";

// Function to save metadata as JSON
void saveMetadata(const std::string& diskName, int sizeMB, int totalBlocks) {
    json metadata;
    metadata["disk_name"] = diskName;
    metadata["size_MB"] = sizeMB;
    metadata["total_blocks"] = totalBlocks;
    metadata["block_size"] = BLOCK_SIZE;
    metadata["bitmap"] = std::string(totalBlocks, '0'); // Initialize bitmap
    metadata["inodes"] = json::array();

    std::ofstream metaFile(META_FILE, std::ios::trunc);
    if (!metaFile) {
        std::cerr << "Error writing metadata file.\n";
        return;
    }
    metaFile << metadata.dump(4);
    metaFile.close();
}

// Function to load metadata
json loadMetadata() {
    std::ifstream metaFile(META_FILE);
    
    if (!metaFile) {
        std::cerr << "Error loading metadata file. Creating a new one...\n";
        return {}; // Retorna un JSON vacío si el archivo no existe
    }

    json metadata;
    try {
        metaFile >> metadata;
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return {}; // Devuelve JSON vacío si el archivo está corrupto
    }
    
    return metadata;
}

// Function to save metadata
void saveMetadata(const json& metadata) {
    std::ofstream metaFile(META_FILE, std::ios::trunc);
    if (!metaFile) {
        std::cerr << "Error saving metadata file.\n";
        return;
    }
    metaFile << metadata.dump(4);
}

// Function to write a block to the disk
void writeBlock(int blockIndex, const std::vector<uint8_t>& data) {
    std::fstream disk(DISK_NAME, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk) {
        std::cerr << "Error opening the disk.\n";
        return;
    }
    disk.seekp(blockIndex * BLOCK_SIZE);
    disk.write(reinterpret_cast<const char*>(data.data()), BLOCK_SIZE);
    disk.close();
}

// Function to read a block from the disk
std::vector<uint8_t> readBlock(int blockIndex) {
    std::ifstream disk(DISK_NAME, std::ios::binary);
    if (!disk) {
        std::cerr << "Error opening the disk.\n";
        return {};
    }
    std::vector<uint8_t> data(BLOCK_SIZE);
    disk.seekg(blockIndex * BLOCK_SIZE);
    disk.read(reinterpret_cast<char*>(data.data()), BLOCK_SIZE);
    disk.close();
    return data;
}

// Function to allocate a free block
int allocateBlock() {
    json metadata = loadMetadata();
    if (!metadata.contains("bitmap") || metadata["bitmap"].is_null()) {
        std::cerr << "Error: Bitmap is missing in metadata.\n";
        return -1;
    }
    std::string bitmap = metadata["bitmap"].get<std::string>();

    for (size_t i = 0; i < bitmap.size(); i++) {
        if (bitmap[i] == '0') { // Free block
            bitmap[i] = '1';
            saveMetadata(metadata);
            return i;
        }
    }
    return -1; // No free blocks available
}

// Function to free a block
void freeBlock(int blockIndex) {
    json metadata = loadMetadata();
    std::string bitmap = metadata["bitmap"].get<std::string>();

    if (blockIndex < 0 || blockIndex >= static_cast<int>(bitmap.size())) {
        std::cerr << "Invalid block index.\n";
        return;
    }
    bitmap[blockIndex] = '0'; // Mark as free
    saveMetadata(metadata);
}

// Function to write a file to the virtual disk
void writeFile(const std::string& fileName, const std::vector<uint8_t>& fileData) {
    json metadata = loadMetadata();
    int numBlocks = (fileData.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    std::vector<int> allocatedBlocks;

    for (int i = 0; i < numBlocks; i++) {
        int block = allocateBlock();
        if (block == -1) {
            std::cerr << "Not enough space on disk.\n";
            return;
        }
        allocatedBlocks.push_back(block);
    }

    // Write data to allocated blocks
    for (size_t i = 0; i < allocatedBlocks.size(); i++) {
        std::vector<uint8_t> blockData(BLOCK_SIZE, 0);
        size_t offset = i * BLOCK_SIZE;
        size_t size = std::min(fileData.size() - offset, (size_t)BLOCK_SIZE);
        std::copy(fileData.begin() + offset, fileData.begin() + offset + size, blockData.begin());
        writeBlock(allocatedBlocks[i], blockData);
    }

    // Save to inode map
    json inode;
    inode["file_name"] = fileName;
    inode["size"] = fileData.size();
    inode["blocks"] = allocatedBlocks;
    metadata["inodes"].push_back(inode);
    saveMetadata(metadata);
    
    std::cout << "File '" << fileName << "' written to disk.\n";
}

// Function to create a disk file
void createDisk(const std::string& diskName, int sizeMB) {
    int totalBlocks = (sizeMB * 1024 * 1024) / BLOCK_SIZE;

    std::ofstream disk(diskName, std::ios::binary | std::ios::trunc);
    if (!disk) {
        std::cerr << "Error creating the disk.\n";
        return;
    }

    std::vector<uint8_t> emptyBlock(BLOCK_SIZE, 0);
    for (int i = 0; i < totalBlocks; i++) {
        disk.write(reinterpret_cast<const char*>(emptyBlock.data()), BLOCK_SIZE);
    }
    disk.close();

    saveMetadata(diskName, sizeMB, totalBlocks);
}

// Function to initialize the filesystem (only if metadata does not exist)
int main() {
    std::string diskName = "filesystem.img";
    int diskSizeMB = 10; // Tamaño del disco en MB

    // Paso 1: Crear el disco si no existe
    std::ifstream diskFile(diskName);
    if (!diskFile) {
        std::cout << "No se encontró el disco. Creándolo...\n";
        createDisk(diskName, diskSizeMB);
    } else {
        std::cout << "Disco encontrado. Cargando metadata...\n";
    }
    diskFile.close();

    // Paso 2: Cargar metadata
    json metadata = loadMetadata();
    
    // Verificar si la metadata está vacía o corrupta
    if (metadata.empty()) {
        std::cerr << "Error: La metadata no pudo ser cargada correctamente.\n";
        return 1;
    }

    // Paso 3: Realizar operaciones con el sistema de archivos
    std::cout << "Sistema de archivos listo.\n";

    return 0;
}