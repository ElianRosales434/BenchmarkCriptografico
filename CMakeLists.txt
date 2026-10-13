cmake_minimum_required(VERSION 3.15)
project(benchmark_criptografia C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
add_definitions(-D_GNU_SOURCE)

# Configuración de salida de binarios
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/scripts/c)

# Búsqueda de OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Búsqueda o ruta personalizada de liboqs
set(LIBOQS_DIR "" CACHE PATH "Ruta al directorio de instalacion o build de liboqs")

if(LIBOQS_DIR)
    include_directories(${LIBOQS_DIR}/include)
    find_library(OQS_LIB NAMES oqs PATHS ${LIBOQS_DIR}/lib ${LIBOQS_DIR}/bin NO_DEFAULT_PATH)
else()
    find_path(OQS_INCLUDE_DIR NAMES oqs/oqs.h)
    find_library(OQS_LIB NAMES oqs)
    if(OQS_INCLUDE_DIR)
        include_directories(${OQS_INCLUDE_DIR})
    endif()
endif()

# ---------------------------------------------------------------------------
# 1. Target: rsa_benchmark
# ---------------------------------------------------------------------------
add_executable(rsa_benchmark scripts/c/rsa_benchmark.c scripts/c/common_bench.h)
target_link_libraries(rsa_benchmark PRIVATE OpenSSL::Crypto)

if(WIN32)
    target_link_libraries(rsa_benchmark PRIVATE psapi)
else()
    target_link_libraries(rsa_benchmark PRIVATE m)
endif()

# ---------------------------------------------------------------------------
# 2. Target: kyber_benchmark
# ---------------------------------------------------------------------------
if(OQS_LIB)
    add_executable(kyber_benchmark scripts/c/kyber_benchmark.c scripts/c/common_bench.h)
    target_link_libraries(kyber_benchmark PRIVATE ${OQS_LIB} OpenSSL::Crypto)
    if(WIN32)
        target_link_libraries(kyber_benchmark PRIVATE psapi)
    else()
        target_link_libraries(kyber_benchmark PRIVATE m)
    endif()
    message(STATUS "kyber_benchmark configurado exitosamente con liboqs: ${OQS_LIB}")
else()
    message(WARNING "No se localizo la biblioteca liboqs ('oqs'). kyber_benchmark no sera compilado. Especifique -DLIBOQS_DIR=<ruta>")
endif()

# ---------------------------------------------------------------------------
# 3. Target: orquestador
# ---------------------------------------------------------------------------
add_executable(orquestador scripts/c/orquestador.c scripts/c/common_bench.h)
if(WIN32)
    target_link_libraries(orquestador PRIVATE psapi)
else()
    target_link_libraries(orquestador PRIVATE m)
endif()

# ---------------------------------------------------------------------------
# 4. Target: inventario (Sin dependencias externas, 100% autónomo)
# ---------------------------------------------------------------------------
add_executable(inventario scripts/c/inventario.c)

