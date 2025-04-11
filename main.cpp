#include "cowfs.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <fstream>
#include "cowfs_metadata.hpp"

// Funcion para mostrar el encabezado de una seccion
void mostrarSeccion(const std::string& titulo) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "  " << titulo << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// Funcion mejorada para mostrar informacion sobre las versiones de un archivo
void mostrarVersionesDetalladas(cowfs::COWFileSystem& fs, const std::string& nombre_archivo) {
    // Abrimos el archivo solo para obtener el descriptor
    cowfs::fd_t fd = fs.open(nombre_archivo, cowfs::FileMode::READ);
    if (fd < 0) {
        std::cout << "Error al abrir archivo para ver versiones: " << nombre_archivo << std::endl;
        return;
    }
    
    // Obtenemos el historial de versiones
    auto versiones = fs.get_version_history(fd);
    
    std::cout << "\nMETADATOS DE VERSIONES DEL ARCHIVO '" << nombre_archivo << "':" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    std::cout << std::left << std::setw(10) << "Version" 
              << std::setw(20) << "Timestamp" 
              << std::setw(15) << "Tamano" 
              << std::setw(15) << "Delta inicio"
              << std::setw(15) << "Delta tamano"
              << "Version previa" << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    
    for (const auto& v : versiones) {
        std::cout << std::left << std::setw(10) << v.version_number 
                  << std::setw(20) << v.timestamp 
                  << std::setw(15) << v.size 
                  << std::setw(15) << v.delta_start
                  << std::setw(15) << v.delta_size
                  << v.prev_version << std::endl;
    }
    std::cout << std::string(70, '-') << std::endl;
    std::cout << "Numero total de versiones: " << versiones.size() << std::endl;
    
    // Cerramos el archivo ya que no lo necesitamos mas
    fs.close(fd);
}

// Funcion para mostrar los metadatos de un archivo
void mostrarMetadatosArchivo(cowfs::COWFileSystem& fs, const std::string& nombre_archivo) {
    // Abrimos el archivo en modo lectura
    cowfs::fd_t fd = fs.open(nombre_archivo, cowfs::FileMode::READ);
    if (fd < 0) {
        std::cout << "Error al abrir el archivo para metadatos: " << nombre_archivo << std::endl;
        return;
    }
    
    // Obtener informacion del archivo
    auto status = fs.get_file_status(fd);
    size_t tamano = fs.get_file_size(fd);
    
    std::cout << "\nMETADATOS DEL ARCHIVO: " << nombre_archivo << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << "Tamano actual: " << tamano << " bytes" << std::endl;
    std::cout << "Version actual: " << status.current_version << std::endl;
    std::cout << "Abierto: " << (status.is_open ? "Si" : "No") << std::endl;
    std::cout << "Modificado: " << (status.is_modified ? "Si" : "No") << std::endl;
    
    // Obtener y mostrar el historial completo de versiones
    mostrarVersionesDetalladas(fs, nombre_archivo);
    
    // Cerramos el archivo
    fs.close(fd);
}

// Funcion para mostrar el contenido de un archivo
void mostrarContenido(cowfs::COWFileSystem& fs, const std::string& nombre_archivo) {
    // Abrimos el archivo en modo lectura
    cowfs::fd_t fd = fs.open(nombre_archivo, cowfs::FileMode::READ);
    if (fd < 0) {
        std::cout << "Error al abrir el archivo para lectura: " << nombre_archivo << std::endl;
        return;
    }
    
    // Obtenemos el tamano del archivo
    size_t tamano = fs.get_file_size(fd);
    if (tamano == 0) {
        std::cout << "El archivo '" << nombre_archivo << "' esta vacio." << std::endl;
        fs.close(fd);
        return;
    }
    
    // Crear un buffer para la lectura
    std::vector<char> buffer(tamano + 1, 0);
    
    // Leer el contenido
    ssize_t bytes_leidos = fs.read(fd, buffer.data(), tamano);
    if (bytes_leidos < 0) {
        std::cout << "Error al leer el archivo '" << nombre_archivo << "'" << std::endl;
        fs.close(fd);
        return;
    }
    
    // Asegurar que el buffer termina en null para imprimirlo como string
    buffer[bytes_leidos] = '\0';
    
    std::cout << "\nCONTENIDO ACTUAL DE '" << nombre_archivo << "' (" << bytes_leidos << " bytes):" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << buffer.data() << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    // Debug auxiliar para mostrar informacion sobre el archivo
    std::cout << "Informacion de depuracion:" << std::endl;
    std::cout << "- Tamano reportado: " << tamano << " bytes" << std::endl;
    std::cout << "- Bytes leidos realmente: " << bytes_leidos << " bytes" << std::endl;
    
    auto status = fs.get_file_status(fd);
    std::cout << "- Esta abierto: " << (status.is_open ? "Si" : "No") << std::endl;
    std::cout << "- Version actual: " << status.current_version << std::endl;
    std::cout << "- Tamano actual segun status: " << status.current_size << " bytes" << std::endl;
    
    // Cerramos el archivo
    fs.close(fd);
}

// Funcion para escribir en un archivo y mostrar resultado
bool escribirVersionArchivo(cowfs::COWFileSystem& fs, const std::string& nombre_archivo, 
                          const std::string& contenido, size_t num_version) {
    std::cout << "\nCREANDO VERSION " << num_version << " DEL ARCHIVO '" << nombre_archivo << "'..." << std::endl;
    
    // Abrimos el archivo en modo escritura
    cowfs::fd_t fd = fs.open(nombre_archivo, cowfs::FileMode::WRITE);
    if (fd < 0) {
        std::cerr << "Error al abrir el archivo para escritura: " << nombre_archivo << std::endl;
        return false;
    }
    
    // Escribimos el contenido
    ssize_t bytes_escritos = fs.write(fd, contenido.c_str(), contenido.length());
    if (bytes_escritos < 0) {
        std::cerr << "Error al escribir en el archivo" << std::endl;
        fs.close(fd);
        return false;
    }
    
    std::cout << "Escritura completada: " << bytes_escritos << " bytes" << std::endl;
    
    // Cerramos el archivo
    fs.close(fd);
    return true;
}

// Funcion para mostrar los archivos en el sistema
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

// Funcion para mostrar el uso de memoria
void mostrarUsoMemoria(cowfs::COWFileSystem& fs) {
    size_t memoria_total = fs.get_total_memory_usage();
    std::cout << "Uso actual de memoria: " << memoria_total << " bytes";
    
    // Convertir a unidades mas legibles si es grande
    if (memoria_total > 1024 * 1024) {
        std::cout << " (" << std::fixed << std::setprecision(2) << (memoria_total / (1024.0 * 1024.0)) << " MB)";
    } else if (memoria_total > 1024) {
        std::cout << " (" << std::fixed << std::setprecision(2) << (memoria_total / 1024.0) << " KB)";
    }
    
    std::cout << std::endl;
}

// Funcion para hacer rollback a una version especifica y mostrar resultados
bool hacerRollback(cowfs::COWFileSystem& fs, const std::string& nombre_archivo, size_t version) {
    std::cout << "\nREALIZANDO ROLLBACK DEL ARCHIVO '" << nombre_archivo << "' A LA VERSION " << version << std::endl;
    
    // Primero mostrar estado antes de rollback
    mostrarMetadatosArchivo(fs, nombre_archivo);
    
    // Abrir archivo para escritura (se requiere para rollback)
    cowfs::fd_t fd = fs.open(nombre_archivo, cowfs::FileMode::WRITE);
    if (fd < 0) {
        std::cerr << "Error al abrir archivo para rollback: " << nombre_archivo << std::endl;
        return false;
    }
    
    // Realizar el rollback
    bool resultado = fs.rollback_to_version(fd, version);
    
    if (resultado) {
        std::cout << "ROLLBACK EXITOSO" << std::endl;
    } else {
        std::cerr << "ERROR AL REALIZAR ROLLBACK" << std::endl;
    }
    
    // Cerrar archivo
    fs.close(fd);
    
    // Mostrar estado después del rollback
    std::cout << "\nESTADO DESPUES DEL ROLLBACK:" << std::endl;
    mostrarMetadatosArchivo(fs, nombre_archivo);
    mostrarContenido(fs, nombre_archivo);
    
    return resultado;
}

int main() {
    try {
        mostrarSeccion("EJEMPLO DE SISTEMA DE ARCHIVOS CON COPY-ON-WRITE (COW)");
        
        // Crear un sistema de archivos de 10MB
        const size_t TAMANO_DISCO = 10 * 1024 * 1024; // 10MB
        cowfs::COWFileSystem fs("cowfs_disk.dat", TAMANO_DISCO);
        
        std::cout << "Sistema de archivos COW inicializado correctamente" << std::endl;
        mostrarUsoMemoria(fs);
        
        //======================================================================
        // DEMOSTRACIÓN 1: CREACIÓN DE MÚLTIPLES VERSIONES DE UN ARCHIVO
        //======================================================================
        mostrarSeccion("DEMOSTRACION 1: CREACION DE MULTIPLES VERSIONES DE UN ARCHIVO");
        
        std::cout << "En esta demostracion, crearemos un archivo y le agregaremos varias versiones." << std::endl;
        std::cout << "Cada version tendra un contenido diferente y se almacenaran solo los cambios (deltas)." << std::endl;
        
        // Crear un nuevo archivo para pruebas de versiones
        std::string nombre_archivo = "prueba_versiones.txt";
        cowfs::fd_t fd = fs.create(nombre_archivo);
        if (fd < 0) {
            std::cerr << "Error al crear el archivo '" << nombre_archivo << "'" << std::endl;
            return 1;
        }
        std::cout << "Archivo '" << nombre_archivo << "' creado con exito (fd=" << fd << ")" << std::endl;
        fs.close(fd);
        
        // Versión 1: Contenido inicial
        std::string contenido_v1 = 
            "VERSION 1: CONTENIDO INICIAL\n"
            "Este es el contenido de la primera version del archivo.\n"
            "Estamos demostrando el funcionamiento del sistema COW.\n"
            "Esta version servira como base para futuras modificaciones.";
            
        if (!escribirVersionArchivo(fs, nombre_archivo, contenido_v1, 1)) {
            return 1;
        }
        
        // Mostrar contenido y versiones después de la escritura
        mostrarContenido(fs, nombre_archivo);
        mostrarVersionesDetalladas(fs, nombre_archivo);
        
        // Versión 2: Adición de texto al principio y al final
        std::string contenido_v2 = 
            "VERSION 2: MODIFICACION PARCIAL\n"
            "Este es el contenido de la primera version del archivo.\n"
            "Estamos demostrando el funcionamiento del sistema COW.\n"
            "Esta version servira como base para futuras modificaciones.\n"
            "LINEA AGREGADA AL FINAL EN LA VERSION 2";
            
        if (!escribirVersionArchivo(fs, nombre_archivo, contenido_v2, 2)) {
            return 1;
        }
        
        // Mostrar contenido y versiones después de la escritura
        mostrarContenido(fs, nombre_archivo);
        mostrarVersionesDetalladas(fs, nombre_archivo);
        
        // Versión 3: Cambio completo del contenido
        std::string contenido_v3 = 
            "VERSION 3: CAMBIO COMPLETO\n"
            "Este es un contenido completamente diferente.\n"
            "Hemos cambiado todo el texto para demostrar como\n"
            "el sistema COW detecta que todo ha cambiado y almacena\n"
            "un nuevo conjunto de bloques para esta version.";
            
        if (!escribirVersionArchivo(fs, nombre_archivo, contenido_v3, 3)) {
            return 1;
        }
        
        // Mostrar contenido y versiones después de la escritura
        mostrarContenido(fs, nombre_archivo);
        mostrarVersionesDetalladas(fs, nombre_archivo);
        
        // Versión 4: Modificación pequeña en medio del texto
        std::string contenido_v4 = 
            "VERSION 3: CAMBIO COMPLETO\n"
            "Este es un contenido completamente diferente.\n"
            "ESTA LINEA HA SIDO MODIFICADA EN LA VERSION 4\n"
            "el sistema COW detecta que todo ha cambiado y almacena\n"
            "un nuevo conjunto de bloques para esta version.";
            
        if (!escribirVersionArchivo(fs, nombre_archivo, contenido_v4, 4)) {
            return 1;
        }
        
        // Mostrar contenido y versiones después de la escritura
        mostrarContenido(fs, nombre_archivo);
        mostrarVersionesDetalladas(fs, nombre_archivo);
        
        // Mostrar uso de memoria después de crear varias versiones
        mostrarUsoMemoria(fs);
        
        // Mostrar metadatos detallados del archivo
        mostrarMetadatosArchivo(fs, nombre_archivo);
        
        //======================================================================
        // DEMOSTRACIÓN 2: ROLLBACK A VERSIONES ANTERIORES
        //======================================================================
        mostrarSeccion("DEMOSTRACION 2: ROLLBACK A VERSIONES ANTERIORES");
        
        std::cout << "En esta demostracion, realizaremos rollback a diferentes versiones" << std::endl;
        std::cout << "del archivo anteriormente creado y observaremos los cambios." << std::endl;
        
        // Mostrar estado actual antes de cualquier rollback
        std::cout << "\nESTADO ACTUAL ANTES DE ROLLBACK:" << std::endl;
        mostrarContenido(fs, nombre_archivo);
        mostrarVersionesDetalladas(fs, nombre_archivo);
        
        // Rollback a versión 2
        if (hacerRollback(fs, nombre_archivo, 2)) {
            std::cout << "\nEl archivo ha vuelto a la version 2." << std::endl;
        }
        
        // Crear una nueva versión después del rollback
        std::string contenido_post_rollback = 
            "VERSION POST-ROLLBACK\n"
            "Este contenido se ha creado despues de hacer rollback a la version 2.\n"
            "Ahora deberia aparecer como una nueva version en el historial.\n"
            "El sistema COW ha descartado todas las versiones posteriores a la 2\n"
            "durante el rollback, y ahora esta es la nueva version mas reciente.";
            
        if (!escribirVersionArchivo(fs, nombre_archivo, contenido_post_rollback, 3)) {
            return 1;
        }
        
        // Mostrar estado final después de rollback y nueva versión
        std::cout << "\nESTADO FINAL DESPUES DE ROLLBACK Y NUEVA VERSION:" << std::endl;
        mostrarContenido(fs, nombre_archivo);
        mostrarVersionesDetalladas(fs, nombre_archivo);

        // Guardar metadatos después de las operaciones
        std::cout << "\nGuardando metadatos del sistema..." << std::endl;
        if (cowfs::MetadataManager::save_and_print_metadata(fs, "version_final")) {
            std::cout << "Metadatos guardados exitosamente" << std::endl;
        } else {
            std::cerr << "Error al guardar los metadatos" << std::endl;
        }

        std::cout << "\nDemostracion del sistema COW completada con exito." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
