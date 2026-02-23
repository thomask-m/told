cmake_minimum_required(VERSION 3.30)
project(told)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wconversion -Wcast-align)

add_subdirectory(src)

