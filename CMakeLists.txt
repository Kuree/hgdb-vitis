cmake_minimum_required(VERSION 3.5)
project(hgdb-vitis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)

add_subdirectory(extern/pybind11)
add_subdirectory(python)
