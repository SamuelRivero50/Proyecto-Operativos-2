#include "cowfs.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>

// Función para mostrar el encabezado de una sección
void mostrarSeccion(const std::string& titulo) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "  " << titulo << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// Función para mostrar información sobre las versiones de un archivo
void mostrarVersiones(cowfs::COWFileSystem& fs, cowfs::fd_t fd, const std::string& nombre_archivo) {
    auto versiones = fs.get_version_history(fd);
    std::cout << "Versiones del archivo '" << nombre_archivo << "':" << std::endl;
    std::cout << std::left << std::setw(10) << "Versión" 
              << std::setw(20) << "Timestamp" 
              << std::setw(15) << "Tamaño" 
              << std::setw(15) << "Delta inicio"
              << "Delta tamaño" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    for (const auto& v : versiones) {
        std::cout << std::left << std::setw(10) << v.version_number 
                  << std::setw(20) << v.timestamp 
                  << std::setw(15) << v.size 
                  << std::setw(15) << v.delta_start
                  << v.delta_size << std::endl;
    }
}

// Función para mostrar el contenido de un archivo
void mostrarContenido(cowfs::COWFileSystem& fs, cowfs::fd_t fd, const std::string& nombre_archivo) {
    size_t tamano = fs.get_file_size(fd);
    if (tamano == 0) {
        std::cout << "El archivo '" << nombre_archivo << "' está vacío." << std::endl;
        return;
    }
    
    // Crear un buffer para la lectura
    std::vector<char> buffer(tamano + 1, 0);
    
    // Leer el contenido
    ssize_t bytes_leidos = fs.read(fd, buffer.data(), tamano);
    if (bytes_leidos < 0) {
        std::cout << "Error al leer el archivo '" << nombre_archivo << "'" << std::endl;
        return;
    }
    
    // Asegurar que el buffer termina en null para imprimirlo como string
    buffer[bytes_leidos] = '\0';
    
    std::cout << "Contenido de '" << nombre_archivo << "' (" << bytes_leidos << " bytes):" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    std::cout << buffer.data() << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
}

// Función para mostrar los archivos en el sistema
void mostrarArchivos(cowfs::COWFileSystem& fs) {
    std::vector<std::string> archivos;
    if (fs.list_files(archivos)) {
        std::cout << "Archivos en el sistema (" << archivos.size() << "):" << std::endl;
        for (const auto& archivo : archivos) {
            std::cout << " - " << archivo << std::endl;
        }
    } else {
        std::cout << "Error al listar archivos." << std::endl;
    }
}

// Función para mostrar el uso de memoria
void mostrarUsoMemoria(cowfs::COWFileSystem& fs) {
    size_t memoria_total = fs.get_total_memory_usage();
    std::cout << "Uso actual de memoria: " << memoria_total << " bytes";
    
    // Convertir a unidades más legibles si es grande
    if (memoria_total > 1024 * 1024) {
        std::cout << " (" << std::fixed << std::setprecision(2) << (memoria_total / (1024.0 * 1024.0)) << " MB)";
    } else if (memoria_total > 1024) {
        std::cout << " (" << std::fixed << std::setprecision(2) << (memoria_total / 1024.0) << " KB)";
    }
    
    std::cout << std::endl;
}

int main() {
    try {
        mostrarSeccion("INICIALIZACIÓN DEL SISTEMA DE ARCHIVOS COW");
        
        // Crear un sistema de archivos de 10MB
        const size_t TAMANO_DISCO = 10 * 1024 * 1024; // 10MB
        cowfs::COWFileSystem fs("cowfs_disk.dat", TAMANO_DISCO);
        
        std::cout << "Sistema de archivos COW inicializado correctamente" << std::endl;
        mostrarUsoMemoria(fs);
        
        // Demostración de operaciones básicas
        mostrarSeccion("OPERACIONES BÁSICAS: CREAR, ESCRIBIR, LEER Y CERRAR");
        
        // Crear un nuevo archivo
        std::string nombre_archivo = "documento.txt";
        cowfs::fd_t fd = fs.create(nombre_archivo);
        if (fd < 0) {
            std::cerr << "Error al crear el archivo '" << nombre_archivo << "'" << std::endl;
            return 1;
        }
        std::cout << "Archivo '" << nombre_archivo << "' creado con éxito (fd=" << fd << ")" << std::endl;
        
        // Escribir contenido inicial (versión 1)
        std::string contenido_v1 = "Este es el contenido inicial del archivo.\n"
                                  "Estamos creando la primera versión del documento.\n"
                                  "Esta versión servirá como base para futuras modificaciones.";
        
        ssize_t bytes_escritos = fs.write(fd, contenido_v1.c_str(), contenido_v1.length());
        if (bytes_escritos < 0) {
            std::cerr << "Error al escribir en el archivo" << std::endl;
            return 1;
        }
        std::cout << "Escrita versión 1: " << bytes_escritos << " bytes" << std::endl;
        
        // Mostrar el contenido y versiones
        mostrarContenido(fs, fd, nombre_archivo);
        mostrarVersiones(fs, fd, nombre_archivo);
        mostrarUsoMemoria(fs);
        
        // Escribir una segunda versión
        std::string contenido_v2 = "Este es el contenido modificado del archivo.\n"
                                  "Estamos creando la segunda versión del documento.\n"
                                  "Se han realizado cambios importantes en esta versión.";
        
        bytes_escritos = fs.write(fd, contenido_v2.c_str(), contenido_v2.length());
        if (bytes_escritos < 0) {
            std::cerr << "Error al escribir la segunda versión" << std::endl;
            return 1;
        }
        std::cout << "Escrita versión 2: " << bytes_escritos << " bytes" << std::endl;
        
        // Mostrar el contenido y versiones actualizados
        mostrarContenido(fs, fd, nombre_archivo);
        mostrarVersiones(fs, fd, nombre_archivo);
        mostrarUsoMemoria(fs);
        
        // Escribir una tercera versión con un cambio pequeño
        std::string contenido_v3 = "Este es el contenido modificado del archivo.\n"
                                  "Estamos creando la tercera versión del documento.\n"
                                  "Se han realizado cambios menores en esta versión.";
        
        bytes_escritos = fs.write(fd, contenido_v3.c_str(), contenido_v3.length());
        if (bytes_escritos < 0) {
            std::cerr << "Error al escribir la tercera versión" << std::endl;
            return 1;
        }
        std::cout << "Escrita versión 3: " << bytes_escritos << " bytes" << std::endl;
        
        // Mostrar el contenido y versiones actualizados
        mostrarContenido(fs, fd, nombre_archivo);
        mostrarVersiones(fs, fd, nombre_archivo);
        
        // Cerrar el archivo
        fs.close(fd);
        std::cout << "Archivo cerrado correctamente" << std::endl;
        
        // Demostración de rollback
        mostrarSeccion("DEMOSTRACIÓN DE ROLLBACK A VERSIÓN ANTERIOR");
        
        // Reabrir el archivo en modo escritura
        fd = fs.open(nombre_archivo, cowfs::FileMode::WRITE);
        if (fd < 0) {
            std::cerr << "Error al abrir el archivo para rollback" << std::endl;
            return 1;
        }
        
        // Mostrar información antes del rollback
        std::cout << "Estado antes del rollback:" << std::endl;
        mostrarContenido(fs, fd, nombre_archivo);
        mostrarVersiones(fs, fd, nombre_archivo);
        
        // Realizar rollback a la versión 1
        std::cout << "Realizando rollback a la versión 1..." << std::endl;
        if (fs.rollback_to_version(fd, 1)) {
            std::cout << "Rollback exitoso" << std::endl;
            
            // Mostrar información después del rollback
            std::cout << "Estado después del rollback:" << std::endl;
            mostrarContenido(fs, fd, nombre_archivo);
            mostrarVersiones(fs, fd, nombre_archivo);
        } else {
            std::cerr << "Error al realizar rollback" << std::endl;
        }
        
        // Cerrar el archivo
        fs.close(fd);
        
        // Demostración de recolección de basura
        mostrarSeccion("DEMOSTRACIÓN DE RECOLECCIÓN DE BASURA");
        
        // Mostrar uso de memoria antes de la recolección
        std::cout << "Uso de memoria antes de la recolección de basura:" << std::endl;
        mostrarUsoMemoria(fs);
        
        // Ejecutar recolector de basura
        std::cout << "Ejecutando recolección de basura..." << std::endl;
        fs.garbage_collect();
        
        // Mostrar uso de memoria después de la recolección
        std::cout << "Uso de memoria después de la recolección de basura:" << std::endl;
        mostrarUsoMemoria(fs);
        
        // Crear varios archivos para demostrar la gestión de múltiples archivos
        mostrarSeccion("GESTIÓN DE MÚLTIPLES ARCHIVOS");
        
        // Crear y escribir en varios archivos
        for (int i = 1; i <= 5; i++) {
            std::string nombre = "archivo" + std::to_string(i) + ".txt";
            std::string contenido = "Contenido del archivo " + std::to_string(i);
            
            fd = fs.create(nombre);
            if (fd >= 0) {
                fs.write(fd, contenido.c_str(), contenido.length());
                fs.close(fd);
                std::cout << "Creado archivo: " << nombre << std::endl;
            }
        }
        
        // Listar todos los archivos
        mostrarArchivos(fs);
        
        // Mostrar uso final de memoria
        mostrarSeccion("RESUMEN FINAL");
        mostrarUsoMemoria(fs);
        
        std::cout << "\nDemostración completada con éxito." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
