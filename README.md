# Documentación de la API y Uso de la Biblioteca COWFS

**Integrantes**:

- Samuel Enrique Rivero Urribarrí
- Paulina Velasquez Londoño
- Samuel Henao Castrillón

## Introducción

La biblioteca COWFS (Copy-on-Write File System) implementa un sistema de archivos que utiliza el mecanismo de Copy-on-Write para gestionar las escrituras y el versionado de archivos. Este sistema garantiza la integridad de los datos y permite acceder al historial de versiones de cada archivo.

## Características Principales

- **Versionado automático**: Cada operación de escritura crea una nueva versión del archivo.
- **Lecturas optimizadas**: Las operaciones de lectura siempre acceden a la última versión disponible.
- **Gestión de memoria eficiente**: Utiliza técnicas de referencia y recolección de basura para minimizar la sobrecarga de almacenamiento.
- **Rollback de versiones**: Permite volver a versiones anteriores de un archivo.
- **Detección de deltas**: Almacena solo los cambios entre versiones para optimizar el uso de espacio.

## Estructura de la Biblioteca

### Constantes Principales

- `BLOCK_SIZE`: 4096 bytes (tamaño de cada bloque de datos)
- `MAX_FILENAM
### Constantes Principales

- `BLOCK_SIZE`: 4096 bytes (tamaño de cada bloque de datos)
- `MAX_FILENAME_LENGTH`: 255 caracteres (longitud máxima de nombre de archivo)
- `MAX_FILES`: 1024 (número máximo de archivos en el sistema)

### Tipos de Datos

- `fd_t`: Tipo para descriptores de archivo (int32_t)
- `FileMode`: Enumeración para los modos de apertura de archivos
  - `READ`: Modo lectura
  - `WRITE`: Modo escritura
  - `CREATE`: Modo creación

### Estructuras de Datos

- `Block`: Representa un bloque de datos en el sistema de archivos
- `VersionInfo`: Almacena información sobre versiones de archivos
- `Inode`: Representa los metadatos de un archivo
- `FileStatus`: Estado actual de un archivo

## Implementación Interna

### Mecanismo Copy-on-Write (COW)

El sistema COWFS implementa el principio de Copy-on-Write de la siguiente manera:

1. **Escritura inicial**: Cuando se crea un archivo por primera vez, se asignan bloques para almacenar sus datos.

2. **Escrituras subsecuentes**: Cuando se modifica un archivo:
   - Se detectan las partes modificadas (deltas) entre la versión anterior y la nueva
   - Solo se crean nuevos bloques para almacenar los datos modificados
   - Los bloques no modificados se comparten con versiones anteriores mediante contadores de referencia
   - Se crea un registro en el historial de versiones con metadatos sobre los cambios

3. **Referencias compartidas**: Cada bloque tiene un contador de referencias que indica cuántas versiones lo utilizan. Cuando un contador llega a cero, el bloque puede ser liberado por el recolector de basura.

### Optimización de Almacenamiento

El sistema emplea varias técnicas para minimizar el uso de memoria:

1. **Detección de deltas**: Solo se almacenan las diferencias entre versiones consecutivas.
2. **Compartición de bloques**: Los bloques no modificados se comparten entre versiones.
3. **Recolección de basura**: Periódicamente se liberan los bloques que ya no están en uso.
4. **Gestión de memoria por bloques**: Se utiliza un sistema de asignación de memoria basado en bloques de tamaño fijo.
5. **Algoritmo de "mejor ajuste"**: Para la asignación de bloques, se utiliza un algoritmo que minimiza la fragmentación.

### Estructuras Internas

#### Lista de Bloques Libres
El sistema mantiene una lista enlazada de bloques libres que se gestiona con las siguientes operaciones:
- Fusión de bloques contiguos (`merge_free_blocks`)
- División de bloques para ajustarse a tamaños específicos (`split_free_block`)
- Búsqueda del bloque con mejor ajuste según el tamaño necesario (`find_best_fit`)

## API Pública

### Funciones Principales

#### Constructor

```cpp
COWFileSystem(const std::string& disk_path, size_t disk_size)
```

Inicializa el sistema de archivos en la ruta especificada con el tamaño indicado.

- **Parámetros**:
  - `disk_path`: Ruta del archivo que representa el disco
  - `disk_size`: Tamaño total del disco en bytes

#### Destructor

```cpp
~COWFileSystem()
```

Libera los recursos y guarda el estado actual en el disco.

#### Operaciones Básicas de Archivos

##### Crear un Archivo

```cpp
fd_t create(const std::string& filename)
```

Crea un nuevo archivo en el sistema.

- **Parámetros**:
  - `filename`: Nombre del archivo a crear
- **Retorno**: Descriptor del archivo creado, o -1 en caso de error

##### Abrir un Archivo

```cpp
fd_t open(const std::string& filename, FileMode mode)
```

Abre un archivo existente en el sistema.

- **Parámetros**:
  - `filename`: Nombre del archivo a abrir
  - `mode`: Modo de apertura (READ, WRITE, CREATE)
- **Retorno**: Descriptor del archivo abierto, o -1 en caso de error

##### Leer de un Archivo

```cpp
ssize_t read(fd_t fd, void* buffer, size_t size)
```

Lee datos de un archivo abierto.

- **Parámetros**:
  - `fd`: Descriptor del archivo
  - `buffer`: Buffer donde se almacenarán los datos leídos
  - `size`: Cantidad de bytes a leer
- **Retorno**: Número de bytes leídos, o -1 en caso de error

##### Escribir en un Archivo

```cpp
ssize_t write(fd_t fd, const void* buffer, size_t size)
```

Escribe datos en un archivo abierto, creando una nueva versión.

- **Parámetros**:
  - `fd`: Descriptor del archivo
  - `buffer`: Buffer con los datos a escribir
  - `size`: Cantidad de bytes a escribir
- **Retorno**: Número de bytes escritos, o -1 en caso de error

##### Cerrar un Archivo

```cpp
int close(fd_t fd)
```

Cierra un archivo abierto.

- **Parámetros**:
  - `fd`: Descriptor del archivo a cerrar
- **Retorno**: 0 si se cerró correctamente, -1 en caso de error

#### Gestión de Versiones

##### Obtener Historial de Versiones

```cpp
std::vector<VersionInfo> get_version_history(fd_t fd)
```

Obtiene el historial de versiones de un archivo.

- **Parámetros**:
  - `fd`: Descriptor del archivo
- **Retorno**: Vector con información de todas las versiones

##### Contar Versiones

```cpp
size_t get_version_count(fd_t fd)
```

Obtiene el número total de versiones de un archivo.

- **Parámetros**:
  - `fd`: Descriptor del archivo
- **Retorno**: Número de versiones

##### Revertir a una Versión Anterior

```cpp
bool revert_to_version(fd_t fd, size_t version)
```

Revierte temporalmente a una versión anterior sin eliminar versiones más recientes.

- **Parámetros**:
  - `fd`: Descriptor del archivo
  - `version`: Número de versión a la que revertir
- **Retorno**: true si se revirtió correctamente, false en caso de error

##### Rollback a una Versión Anterior

```cpp
bool rollback_to_version(fd_t fd, size_t version_number)
```

Revierte permanentemente a una versión anterior, eliminando todas las versiones posteriores.

- **Parámetros**:
  - `fd`: Descriptor del archivo
  - `version_number`: Número de versión a la que hacer rollback
- **Retorno**: true si el rollback fue exitoso, false en caso de error

#### Operaciones del Sistema de Archivos

##### Listar Archivos

```cpp
bool list_files(std::vector<std::string>& files)
```

Obtiene una lista de todos los archivos en el sistema.

- **Parámetros**:
  - `files`: Vector donde se almacenarán los nombres de los archivos
- **Retorno**: true si la operación fue exitosa, false en caso de error

##### Obtener Tamaño de Archivo

```cpp
size_t get_file_size(fd_t fd)
```

Obtiene el tamaño actual de un archivo.

- **Parámetros**:
  - `fd`: Descriptor del archivo
- **Retorno**: Tamaño del archivo en bytes

##### Obtener Estado del Archivo

```cpp
FileStatus get_file_status(fd_t fd)
```

Obtiene el estado actual de un archivo.

- **Parámetros**:
  - `fd`: Descriptor del archivo
- **Retorno**: Estructura con el estado del archivo

#### Gestión de Memoria

##### Obtener Uso Total de Memoria

```cpp
size_t get_total_memory_usage()
```

Calcula y devuelve el uso total de memoria del sistema de archivos.

- **Retorno**: Uso total de memoria en bytes

##### Recolección de Basura

```cpp
void garbage_collect()
```

Ejecuta el recolector de basura para liberar bloques no utilizados.

## Consejos para el Uso Eficiente

1. **Cierre adecuado de archivos**: Siempre cierre los archivos después de usarlos para garantizar que los cambios se guarden correctamente.

2. **Gestión de versiones**: Realice `rollback_to_version` solo cuando sea necesario, ya que elimina permanentemente versiones posteriores.

3. **Uso de la recolección de basura**: Ejecute `garbage_collect()` periódicamente cuando el sistema no esté bajo carga intensa para liberar recursos.

4. **Tamaño de las escrituras**: Intente agrupar modificaciones pequeñas en operaciones de escritura más grandes para minimizar la fragmentación.

5. **Memoria disponible**: Monitoree el uso de memoria con `get_total_memory_usage()` para evitar agotarla.

## Implementación Avanzada

### Procedimiento de Rollback

El proceso de rollback a una versión anterior implica:

1. Verificar que la versión solicitada exista
2. Eliminar todas las versiones posteriores a la solicitada
3. Decrementar los contadores de referencia de los bloques exclusivos de las versiones eliminadas
4. Actualizar los metadatos del archivo para reflejar el estado de la versión seleccionada

### Recolección de Basura

El algoritmo de garbage collector:

1. Identifica todos los bloques actualmente en uso
2. Marca como libres los bloques con contador de referencias igual a cero
3. Combina bloques libres contiguos para reducir la fragmentación
4. Actualiza la lista de bloques libres

## Ejemplo de Uso Básico

```cpp
#include "cowfs.hpp"
#include <iostream>

int main() {
    // Inicializar sistema de archivos
    cowfs::COWFileSystem fs("mi_disco.dat", 1024 * 1024);  // 1MB

    // Crear un archivo
    cowfs::fd_t fd = fs.create("ejemplo.txt");
    if (fd < 0) {
        std::cerr << "Error al crear el archivo" << std::endl;
        return 1;
    }

    // Escribir datos (crea versión 1)
    const char* datos1 = "Hola, mundo!";
    fs.write(fd, datos1, strlen(datos1));

    // Cerrar archivo
    fs.close(fd);

    // Reabrir para lectura
    fd = fs.open("ejemplo.txt", cowfs::FileMode::READ);
    
    // Leer contenido
    char buffer[100] = {0};
    fs.read(fd, buffer, 100);
    std::cout << "Contenido del archivo: " << buffer << std::endl;

    // Cerrar archivo
    fs.close(fd);

    return 0;
}
```

## Consideraciones de Rendimiento
 
- El sistema está optimizado para minimizar el uso de almacenamiento mediante la detección de deltas entre versiones.
- La recolección de basura debe ejecutarse periódicamente para liberar bloques no utilizados.
- Las operaciones de escritura son más costosas que las de lectura debido a la creación de nuevas versiones.

## Limitaciones

- Tamaño máximo de archivos limitado por el tamaño total del disco.
- Número máximo de archivos definido por la constante MAX_FILES (1024).
- No se recomienda para sistemas con alta concurrencia de escritura.
- No se recomienda para versiones de archivos mas de 4096 bytes
