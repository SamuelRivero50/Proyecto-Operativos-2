#include "cowfs.hpp"
#include "cowfs_metadata.hpp"
#include <iostream>
#include <string>

int main() {
    try {
        // Crear un nuevo sistema de archivos
        cowfs::COWFileSystem fs;
        
        // Formatear el sistema de archivos
        if (!fs.format()) {
            std::cerr << "Error al formatear el sistema de archivos" << std::endl;
            return 1;
        }

        // Crear un archivo de prueba
        std::string filename = "test.txt";
        cowfs::fd_t fd = fs.create(filename);
        if (fd < 0) {
            std::cerr << "Error al crear el archivo" << std::endl;
            return 1;
        }

        // Escribir datos en el archivo
        std::string data = "Este es un archivo de prueba";
        ssize_t bytes_written = fs.write(fd, data.c_str(), data.size());
        if (bytes_written < 0) {
            std::cerr << "Error al escribir en el archivo" << std::endl;
            return 1;
        }
        std::cout << "Bytes escritos: " << bytes_written << std::endl;

        // Cerrar el archivo
        fs.close(fd);

        // Guardar metadatos versión 1
        if (!cowfs::MetadataManager::save_and_print_metadata(fs, "v1")) {
            std::cerr << "Error al guardar metadatos v1" << std::endl;
            return 1;
        }

        // Abrir el archivo y modificar
        fd = fs.open(filename, cowfs::FileMode::WRITE);
        if (fd < 0) {
            std::cerr << "Error al abrir el archivo" << std::endl;
            return 1;
        }

        // Escribir nuevos datos
        data = "Este es un archivo de prueba modificado";
        bytes_written = fs.write(fd, data.c_str(), data.size());
        if (bytes_written < 0) {
            std::cerr << "Error al escribir en el archivo" << std::endl;
            return 1;
        }
        std::cout << "Bytes escritos: " << bytes_written << std::endl;

        // Cerrar el archivo
        fs.close(fd);

        // Guardar metadatos versión 2
        if (!cowfs::MetadataManager::save_and_print_metadata(fs, "v2")) {
            std::cerr << "Error al guardar metadatos v2" << std::endl;
            return 1;
        }

        // Verificar el historial de versiones
        fd = fs.open(filename, cowfs::FileMode::READ);
        if (fd < 0) {
            std::cerr << "Error al abrir el archivo para lectura" << std::endl;
            return 1;
        }

        auto version_history = fs.get_version_history(fd);
        std::cout << "\nHistorial de versiones:" << std::endl;
        for (const auto& version : version_history) {
            std::cout << "  Versión " << version.version_number 
                      << " (Tamaño: " << version.size 
                      << ", Bloque: " << version.block_index 
                      << ", Fecha: " << version.timestamp << ")" << std::endl;
        }

        // Leer el contenido actual
        char buffer[1024];
        ssize_t bytes_read = fs.read(fd, buffer, sizeof(buffer));
        if (bytes_read < 0) {
            std::cerr << "Error al leer el archivo" << std::endl;
            return 1;
        }
        buffer[bytes_read] = '\0';
        std::cout << "\nContenido actual del archivo: " << buffer << std::endl;

        fs.close(fd);

        std::cout << "\nPrueba completada exitosamente" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error en la prueba: " << e.what() << std::endl;
        return 1;
    }
} 