cmake_minimum_required(VERSION 3.28)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(aion VERSION 0.1.0 LANGUAGES C CXX)

option(AION_BUILD_BENCHMARKS "Build Aion synthetic benchmark binaries" OFF)
option(AION_BUILD_TESTS "Build Aion test binaries" OFF)

# Compiler Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unknown-pragmas)
endif()


# Using gtest for
# Output directories for binaries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src)

if (AION_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if (AION_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
# add_subdirectory(examples) # later
