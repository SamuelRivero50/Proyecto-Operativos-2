cmake_minimum_required(VERSION 3.10)
project(COWFileSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configurar compilador
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Archivos fuente
set(SOURCES
    cowfs.cpp
    cowfs_metadata.cpp
    test_all.cpp
)

# Archivos de encabezado
set(HEADERS
    cowfs.hpp
    cowfs_metadata.hpp
)

# Crear ejecutable
add_executable(cowfs_test ${SOURCES} ${HEADERS})

# Enlazar con bibliotecas necesarias
target_link_libraries(cowfs_test) 