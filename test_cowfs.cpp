#include "cowfs.hpp"
#include <iostream>
#include <string>
#include <cassert>
#include <vector>

using namespace cowfs;

void print_memory_stats(COWFileSystem& fs) {
    std::cout << "Uso total de memoria: " << fs.get_total_memory_usage() << " bytes" << std::endl;
}

void test_file_operations(COWFileSystem& fs) {
    std::cout << "\n=== Prueba de operaciones básicas y fragmentación ===" << std::endl;
    
    // Crear varios archivos para fragmentar el espacio
    const char* test_data1 = "Contenido del archivo 1";
    const char* test_data2 = "Contenido del archivo 2";
    const char* test_data3 = "Contenido del archivo 3";
    
    // Crear y escribir archivos
    fd_t fd1 = fs.create("test1.txt");
    fd_t fd2 = fs.create("test2.txt");
    fd_t fd3 = fs.create("test3.txt");
    
    assert(fd1 >= 0 && fd2 >= 0 && fd3 >= 0);
    
    fs.write(fd1, test_data1, strlen(test_data1));
    fs.write(fd2, test_data2, strlen(test_data2));
    fs.write(fd3, test_data3, strlen(test_data3));
    
    print_memory_stats(fs);
    
    // Cerrar archivos
    fs.close(fd1);
    fs.close(fd2);
    fs.close(fd3);
}

void test_cow_versioning(COWFileSystem& fs) {
    std::cout << "\n=== Prueba de versionado COW ===" << std::endl;
    
    // Crear y modificar un archivo varias veces
    fd_t fd = fs.create("versioned.txt");
    assert(fd >= 0);
    
    // Versión 1
    const char* v1 = "Versión 1 del archivo";
    fs.write(fd, v1, strlen(v1));
    print_memory_stats(fs);
    
    // Versión 2
    const char* v2 = "Versión 2 modificada del archivo";
    fs.write(fd, v2, strlen(v2));
    print_memory_stats(fs);
    
    // Versión 3
    const char* v3 = "Versión 3 final del archivo con más contenido";
    fs.write(fd, v3, strlen(v3));
    print_memory_stats(fs);
    
    // Verificar versiones
    auto status = fs.get_file_status(fd);
    std::cout << "Número de versiones: " << status.current_version << std::endl;
    
    fs.close(fd);
}

void test_fragmentation_and_gc(COWFileSystem& fs) {
    std::cout << "\n=== Prueba de fragmentación y recolección de basura ===" << std::endl;
    
    // Crear y eliminar archivos para generar fragmentación
    std::vector<fd_t> fds;
    
    // Crear 10 archivos pequeños
    for (int i = 0; i < 10; i++) {
        std::string filename = "temp" + std::to_string(i) + ".txt";
        fd_t fd = fs.create(filename.c_str());
        assert(fd >= 0);
        
        std::string content = "Contenido del archivo temporal " + std::to_string(i);
        fs.write(fd, content.c_str(), content.length());
        fds.push_back(fd);
    }
    
    print_memory_stats(fs);
    
    // Cerrar y eliminar archivos alternos para crear fragmentación
    for (int i = 0; i < 10; i += 2) {
        fs.close(fds[i]);
        // Aquí normalmente iría delete/unlink, pero no está implementado en la interfaz
    }
    
    // Ejecutar recolección de basura
    fs.garbage_collect();
    print_memory_stats(fs);
    
    // Crear un archivo grande para ver si usa el espacio liberado
    fd_t fd_large = fs.create("large_file.txt");
    assert(fd_large >= 0);
    
    std::string large_content(8192, 'X'); // 8KB de contenido
    fs.write(fd_large, large_content.c_str(), large_content.length());
    
    print_memory_stats(fs);
}

int main() {
    try {
        // Crear sistema de archivos de prueba (1MB)
        COWFileSystem fs("test_fs.bin", 1024 * 1024);
        
        test_file_operations(fs);
        test_cow_versioning(fs);
        test_fragmentation_and_gc(fs);
        
        std::cout << "\nTodas las pruebas completadas exitosamente!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 