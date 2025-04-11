#include "cowfs.hpp"
#include "cowfs_metadata.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;
using namespace cowfs;  // Agregar el namespace

// Función para limpiar archivos de prueba
void cleanup_test_files() {
    std::vector<std::string> test_files = {
        "test_fs_incremental.bin",
        "test_fs_shared.bin",
        "test_fs_gc.bin",
        "test_fs_metadata.bin",
        "test_fs_rollback.bin",
        "metadata_initial.json",
        "metadata_final.json"
    };
    
    for (const auto& file : test_files) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }
}

// Función para obtener timestamp actual
std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Prueba de versiones incrementales
void test_incremental_versions() {
    std::cout << "\n=== Prueba de versiones incrementales ===" << std::endl;
    
    COWFileSystem fs("test_fs_incremental.bin", 1024 * 1024);
    fd_t fd = fs.create("test.txt");
    
    if (fd < 0) {
        std::cerr << "Error al crear archivo" << std::endl;
        return;
    }
    
    // Escribir contenido inicial
    std::string content1 = "Contenido inicial del archivo";
    ssize_t result = fs.write(fd, content1.c_str(), content1.size());
    std::cout << "Escritura inicial: " << result << " bytes" << std::endl;
    
    // Modificar contenido
    std::string content2 = "Contenido modificado del archivo";
    result = fs.write(fd, content2.c_str(), content2.size());
    std::cout << "Escritura modificada: " << result << " bytes" << std::endl;
    
    // Verificar versiones
    auto versions = fs.get_version_history(fd);
    std::cout << "Número de versiones: " << versions.size() << std::endl;
    
    fs.close(fd);
}

// Prueba de bloques compartidos
void test_shared_blocks() {
    std::cout << "\n=== Prueba de bloques compartidos ===" << std::endl;
    
    COWFileSystem fs("test_fs_shared.bin", 1024 * 1024);
    
    // Crear archivo con contenido repetitivo
    fd_t fd1 = fs.create("test.txt");
    std::string large_content(8192, 'A');
    ssize_t result = fs.write(fd1, large_content.c_str(), large_content.size());
    std::cout << "Escritura archivo 1: " << result << " bytes" << std::endl;
    
    // Crear segundo archivo con mismo contenido
    fd_t fd2 = fs.create("test2.txt");
    result = fs.write(fd2, large_content.c_str(), large_content.size());
    std::cout << "Escritura archivo 2: " << result << " bytes" << std::endl;
    
    // Verificar uso de memoria
    std::cout << "Uso de memoria total: " << fs.get_total_memory_usage() << " bytes" << std::endl;
    
    fs.close(fd1);
    fs.close(fd2);
}

// Prueba de recolección de basura
void test_garbage_collection() {
    std::cout << "\n=== Prueba de recolección de basura ===" << std::endl;
    
    COWFileSystem fs("test_fs_gc.bin", 1024 * 1024);
    
    // Crear varios archivos
    std::vector<fd_t> fds;
    for (int i = 0; i < 5; i++) {
        std::string filename = "test" + std::to_string(i) + ".txt";
        fd_t fd = fs.create(filename);
        if (fd >= 0) {
            fds.push_back(fd);
            std::string content = "Contenido del archivo " + std::to_string(i);
            fs.write(fd, content.c_str(), content.size());
        }
    }
    
    // Cerrar y eliminar algunos archivos
    for (size_t i = 0; i < fds.size(); i += 2) {
        fs.close(fds[i]);
    }
    
    // Ejecutar recolección de basura
    std::cout << "Uso de memoria antes de GC: " << fs.get_total_memory_usage() << " bytes" << std::endl;
    fs.garbage_collect();
    std::cout << "Uso de memoria después de GC: " << fs.get_total_memory_usage() << " bytes" << std::endl;
    
    // Cerrar archivos restantes
    for (size_t i = 1; i < fds.size(); i += 2) {
        fs.close(fds[i]);
    }
}

// Prueba de metadatos
void test_metadata() {
    std::cout << "\n=== Prueba de metadatos ===" << std::endl;
    
    COWFileSystem fs("test_fs_metadata.bin", 1024 * 1024);
    MetadataManager metadata;
    
    // Crear y modificar archivos
    fd_t fd = fs.create("test.txt");
    std::string content = "Contenido de prueba";
    fs.write(fd, content.c_str(), content.size());
    
    // Guardar metadatos iniciales
    metadata.save_and_print_metadata(fs, "initial");
    
    // Modificar archivo
    content = "Contenido modificado";
    fs.write(fd, content.c_str(), content.size());
    
    // Guardar metadatos finales
    metadata.save_and_print_metadata(fs, "final");
    
    fs.close(fd);
}

// Prueba de rollback
void test_rollback() {
    std::cout << "\n=== Prueba de rollback ===" << std::endl;
    
    COWFileSystem fs("test_fs_rollback.bin", 1024 * 1024);
    fd_t fd = fs.create("test.txt");
    
    // Escribir varias versiones del archivo
    const char* version1 = "Esta es la versión 1";
    const char* version2 = "Esta es la versión 2 modificada";
    const char* version3 = "Esta es la versión 3 con más cambios";
    
    // Escribir versión 1
    fs.write(fd, version1, strlen(version1));
    std::cout << "Versión 1 escrita" << std::endl;
    
    // Escribir versión 2
    fs.write(fd, version2, strlen(version2));
    std::cout << "Versión 2 escrita" << std::endl;
    
    // Escribir versión 3
    fs.write(fd, version3, strlen(version3));
    std::cout << "Versión 3 escrita" << std::endl;
    
    // Hacer rollback a la versión 1
    std::cout << "Intentando rollback a versión 1..." << std::endl;
    if (fs.rollback_to_version(fd, 1)) {
        std::cout << "Rollback exitoso" << std::endl;
        
        // Leer el contenido actual
        char buffer[100];
        ssize_t bytes_read = fs.read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::cout << "Contenido actual: " << buffer << std::endl;
        }
    } else {
        std::cout << "Error en el rollback" << std::endl;
    }
    
    fs.close(fd);
}

int main() {
    try {
        std::cout << "=== Iniciando pruebas del sistema de archivos COW ===" << std::endl;
        
        // Limpiar archivos de prueba anteriores
        cleanup_test_files();
        
        // Ejecutar pruebas
        test_incremental_versions();
        test_shared_blocks();
        test_garbage_collection();
        test_metadata();
        test_rollback();  // Agregar la nueva prueba
        
        std::cout << "\n=== Todas las pruebas completadas exitosamente ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 