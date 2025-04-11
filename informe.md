# Informe del Proyecto: Sistema de Archivos Copy-on-Write (COWFS)

## Resumen Ejecutivo

Este proyecto ha logrado desarrollar con éxito una biblioteca que implementa un sistema de archivos con mecanismo Copy-on-Write (COW) para la gestión eficiente de versiones de archivos. La biblioteca COWFS proporciona una alternativa a los sistemas de escritura tradicionales, permitiendo mantener un historial completo de versiones de cada archivo mientras optimiza el uso de memoria mediante técnicas avanzadas de gestión de almacenamiento.

## Objetivos del Proyecto

El proyecto se propuso alcanzar los siguientes objetivos:

1. Implementar un sistema de archivos que utilice el mecanismo Copy-on-Write.
2. Proporcionar versionado automático de archivos con cada operación de escritura.
3. Garantizar la integridad de los datos mediante la preservación de versiones anteriores.
4. Optimizar el uso de memoria mediante técnicas de referencia compartida y detección de deltas.
5. Ofrecer una interfaz de programación sencilla y familiar para los desarrolladores.

## Implementación y Arquitectura

### Componentes Principales

1. **Gestión de Archivos**: Implementación de operaciones básicas (crear, abrir, leer, escribir, cerrar).
2. **Sistema de Versionado**: Mantenimiento automático de versiones con cada escritura.
3. **Optimización de Almacenamiento**: Detección de deltas y compartición de bloques.
4. **Gestión de Memoria**: Asignación eficiente de bloques y recolección de basura.

### Mecanismo Copy-on-Write

Se implementó el principio COW de forma que las modificaciones a los archivos nunca sobrescriben datos existentes. En su lugar, se crean nuevas versiones que solo almacenan las diferencias (deltas) respecto a versiones anteriores, compartiendo los bloques no modificados mediante un sistema de contadores de referencia.

### Algoritmos Clave

1. **Detección de Deltas**: Identifica exactamente qué partes de un archivo han cambiado entre versiones.
2. **Mejor Ajuste (Best-Fit)**: Algoritmo para la asignación de bloques que minimiza la fragmentación.
3. **Fusión de Bloques Libres**: Combina bloques contiguos para mejorar la eficiencia de almacenamiento.
4. **Recolección de Basura**: Identifica y libera bloques que ya no están referenciados.

## Hallazgos y Resultados


### Rendimiento

- **Lectura**: Las operaciones de lectura mantienen un rendimiento similar a los sistemas tradicionales, accediendo siempre a la última versión disponible.
- **Escritura**: Las operaciones de escritura son más costosas debido a la creación de nuevas versiones y la detección de deltas, pero el impacto se minimiza mediante la optimización del almacenamiento.
- **Rollback**: El retorno a versiones anteriores es eficiente y no requiere reconstrucción completa de archivos.

### Integridad de Datos

El sistema garantiza que ninguna operación de escritura pueda corromper versiones anteriores, proporcionando un mecanismo robusto para:
- Recuperación ante errores
- Análisis histórico de cambios
- Reversión a estados previos conocidos

## Limitaciones y Desafíos

Durante el desarrollo del proyecto se identificaron las siguientes limitaciones:

1. **Sobrecarga de Memoria**: Aunque optimizado, el sistema requiere más espacio que uno sin versionado.
2. **Complejidad de Implementación**: La gestión de referencias y detección de deltas aumenta la complejidad del código.
3. **Escalabilidad**: El sistema está diseñado para un número limitado de archivos y un tamaño máximo de disco.
4. **Concurrencia**: No se han implementado mecanismos avanzados para escrituras concurrentes.
5. **Bloques**: El programa esta diseñado para archivos unicamente de 4096 bytes, por lo que si hay un archivo de más bytes se dificulta su versionado.


## Conclusiones

El proyecto ha cumplido satisfactoriamente sus objetivos, implementando un sistema de archivos COW funcional que:

1. **Preserva la Historia**: Mantiene automáticamente un historial completo de versiones.
2. **Ofrece Flexibilidad**: Permite revertir a cualquier versión anterior.
3. **Garantiza Integridad**: Asegura que los datos nunca sean sobrescritos o corrompidos.

La biblioteca COWFS demuestra la aplicabilidad práctica de los conceptos de virtualización de memoria y sistemas de archivos en la creación de herramientas de gestión de datos eficientes y robustas.

## Aprendizajes

Este proyecto ha permitido aplicar y profundizar conocimientos en:

1. **Virtualización de Memoria**: Aplicación práctica de conceptos de gestión de memoria virtual.
2. **Sistemas de Archivos**: Implementación de mecanismos avanzados de almacenamiento.
3. **Optimización de Recursos**: Técnicas para la utilización eficiente del almacenamiento.
4. **Estructuras de Datos**: Uso de estructuras complejas para la gestión de metadatos y bloques.

La biblioteca COWFS demuestra cómo los principios teóricos de los sistemas operativos pueden aplicarse para crear soluciones prácticas a problemas reales de gestión de datos.
