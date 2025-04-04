#include "cowfs.hpp"
#include <iostream>
#include <string>
#include <cassert>
#include <vector>
#include <cstring>
#include <filesystem>

using namespace cowfs;
namespace fs = std::filesystem;

void cleanup_test_files() {
    std::vector<std::string> files_to_remove = {
        "test_fs_incremental.bin",
        "test_fs_shared.bin",
        "test_fs_gc.bin"
    };
    
    for (const auto& file : files_to_remove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }
}

void print_memory_stats(COWFileSystem& fs) {
    std::cout << "\nEstadísticas de memoria:" << std::endl;
    std::cout << "Uso total de memoria: " << fs.get_total_memory_usage() << " bytes" << std::endl;
}

void test_incremental_versions() {
    std::cout << "\n=== Prueba de versiones incrementales ===" << std::endl;
    
    // Crear sistema de archivos de prueba (1MB)
    COWFileSystem fs("test_fs_incremental.bin", 1024 * 1024);
    
    // Crear un archivo y escribir contenido inicial
    const char* initial_content = "Este es el contenido inicial del archivo.";
    fd_t fd = fs.create("test.txt");
    assert(fd >= 0);
    
    std::cout << "\n1. Escribiendo contenido inicial" << std::endl;
    ssize_t written = fs.write(fd, initial_content, strlen(initial_content));
    assert(written == strlen(initial_content));
    print_memory_stats(fs);
    
    // Modificar solo una parte del contenido
    const char* modified_content = "Este es el contenido MODIFICADO del archivo.";
    std::cout << "\n2. Modificando parte del contenido" << std::endl;
    written = fs.write(fd, modified_content, strlen(modified_content));
    assert(written == strlen(modified_content));
    print_memory_stats(fs);
    
    // Agregar contenido al final
    const char* appended_content = "Este es el contenido MODIFICADO del archivo. Y AQUÍ HAY MÁS CONTENIDO.";
    std::cout << "\n3. Agregando contenido al final" << std::endl;
    written = fs.write(fd, appended_content, strlen(appended_content));
    assert(written == strlen(appended_content));
    print_memory_stats(fs);
    
    // Verificar historial de versiones
    auto versions = fs.get_version_history(fd);
    std::cout << "\nHistorial de versiones:" << std::endl;
    for (const auto& version : versions) {
        std::cout << "Versión " << version.version_number 
                  << ":\n  Tamaño: " << version.size
                  << "\n  Delta inicio: " << version.delta_start
                  << "\n  Delta tamaño: " << version.delta_size
                  << "\n  Timestamp: " << version.timestamp 
                  << std::endl;
    }
    
    fs.close(fd);
}

void test_shared_blocks() {
    std::cout << "\n=== Prueba de bloques compartidos ===" << std::endl;
    
    COWFileSystem fs("test_fs_shared.bin", 1024 * 1024);
    
    // Crear un archivo grande con contenido repetitivo
    std::string large_content(8192, 'A'); // 8KB de 'A's
    fd_t fd = fs.create("large.txt");
    assert(fd >= 0);
    
    std::cout << "\n1. Escribiendo archivo grande inicial" << std::endl;
    ssize_t written = fs.write(fd, large_content.c_str(), large_content.size());
    assert(written == large_content.size());
    print_memory_stats(fs);
    
    // Modificar solo una pequeña parte en el medio
    large_content[4000] = 'B';
    large_content[4001] = 'B';
    large_content[4002] = 'B';
    
    std::cout << "\n2. Modificando solo 3 bytes en el medio" << std::endl;
    written = fs.write(fd, large_content.c_str(), large_content.size());
    assert(written == large_content.size());
    print_memory_stats(fs);
    
    // Verificar que solo se almacenó el delta
    auto versions = fs.get_version_history(fd);
    std::cout << "\nHistorial de versiones del archivo grande:" << std::endl;
    for (const auto& version : versions) {
        std::cout << "Versión " << version.version_number 
                  << ":\n  Tamaño: " << version.size
                  << "\n  Delta inicio: " << version.delta_start
                  << "\n  Delta tamaño: " << version.delta_size
                  << std::endl;
    }
    
    fs.close(fd);
}

void test_garbage_collection() {
    std::cout << "\n=== Prueba de recolección de basura ===" << std::endl;
    
    COWFileSystem fs("test_fs_gc.bin", 1024 * 1024);
    
    // Crear varios archivos y versiones
    std::vector<fd_t> fds;
    for (int i = 0; i < 5; i++) {
        std::string filename = "file" + std::to_string(i) + ".txt";
        fd_t fd = fs.create(filename);
        assert(fd >= 0);
        fds.push_back(fd);
        
        // Crear múltiples versiones
        for (int v = 0; v < 3; v++) {
            std::string content = "Contenido " + std::to_string(v) + " del archivo " + std::to_string(i);
            ssize_t written = fs.write(fd, content.c_str(), content.length());
            assert(written == content.length());
        }
    }
    
    std::cout << "\n1. Estado inicial" << std::endl;
    print_memory_stats(fs);
    
    // Cerrar algunos archivos
    for (size_t i = 0; i < fds.size(); i += 2) {
        fs.close(fds[i]);
    }
    
    std::cout << "\n2. Después de cerrar archivos" << std::endl;
    print_memory_stats(fs);
    
    // Ejecutar recolección de basura
    fs.garbage_collect();
    
    std::cout << "\n3. Después de garbage collection" << std::endl;
    print_memory_stats(fs);
}

int main() {
    try {
        cleanup_test_files();
        
        test_incremental_versions();
        test_shared_blocks();
        test_garbage_collection();
        
        std::cout << "\n¡Todas las pruebas completadas exitosamente!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 