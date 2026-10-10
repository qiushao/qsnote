cmake_minimum_required(VERSION 3.16)

project(sonnet VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(data)
add_subdirectory(src)
