#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

#define BLOCK_SIZE 4096  // Tamaño de cada bloque en bytes

struct Superblock {
    int total_blocks;
    int used_blocks;
    int inode_start;
    int data_start;
};

// Función para crear un disco virtual
bool create_virtual_disk(const std::string& filename, int size_mb) {
    int total_blocks = (size_mb * 1024 * 1024) / BLOCK_SIZE;

    // Abrir el archivo binario para escritura
    std::ofstream disk(filename, std::ios::binary);
    if (!disk) {
        std::cerr << "Error: No se pudo crear el archivo del disco virtual.\n";
        return false;
    }

    // Inicializar el superbloque
    Superblock sb;
    sb.total_blocks = total_blocks;
    sb.used_blocks = 0;
    sb.inode_start = 1;  // Bloque 0 es el superbloque
    sb.data_start = total_blocks / 10;  // Reservamos 10% para inodos

    // Escribir el superbloque en el bloque 0
    disk.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));

    // Inicializar bitmap de bloques (1 bit por bloque)
    std::vector<uint8_t> bitmap(total_blocks / 8, 0);
    disk.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    // Inicializar espacio de inodos (10% del disco)
    std::vector<uint8_t> inodes(sb.data_start * BLOCK_SIZE, 0);
    disk.write(reinterpret_cast<const char*>(inodes.data()), inodes.size());

    // Inicializar espacio de datos (resto del disco)
    std::vector<uint8_t> empty_block(BLOCK_SIZE, 0);
    for (int i = sb.data_start; i < total_blocks; i++) {
        disk.write(reinterpret_cast<const char*>(empty_block.data()), BLOCK_SIZE);
    }

    disk.close();
    std::cout << "Disco virtual creado: " << filename << " (" << size_mb << " MB)\n";
    return true;
}

// Prueba de creación del disco virtual
int main() {
    create_virtual_disk("mi_disco.img", 50);  // Crea un disco de 50MB
    return 0;
}
