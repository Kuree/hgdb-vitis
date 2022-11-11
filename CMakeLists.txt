cmake_minimum_required(VERSION 3.5)
project(hgdb-vitis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
find_package(LLVMV3 REQUIRED)
find_package(LLVMV10 REQUIRED)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_compile_options(-DDEBUG)
endif ()

add_subdirectory(extern)
add_subdirectory(python)
