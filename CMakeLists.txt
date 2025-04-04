cmake_minimum_required(VERSION 3.10)
project(cowfs)

# Configurar C++ estándar
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Descargar y configurar nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Archivos fuente
set(SOURCES
    cowfs.cpp
    cowfs_metadata.cpp
)

# Archivos de encabezado
set(HEADERS
    cowfs.hpp
    cowfs_metadata.hpp
)

# Crear la biblioteca principal
add_library(cowfs STATIC ${SOURCES} ${HEADERS})

# Crear el ejecutable de prueba
add_executable(test_metadata test_metadata.cpp)
target_link_libraries(test_metadata cowfs)

# Configurar el directorio de salida
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Vincular con la biblioteca JSON
target_link_libraries(cowfs PRIVATE nlohmann_json::nlohmann_json) 