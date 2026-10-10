cmake_minimum_required(VERSION 3.16)
project(sonnet-data VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt version to use (5 or 6), defaults to 6")
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core)
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} OPTIONAL_COMPONENTS Core5Compat)

if ((QT_DEFAULT_MAJOR_VERSION GREATER 5))
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

add_executable(gentrigrams
    gentrigrams_src/gentrigrams.cpp
)
target_compile_definitions(gentrigrams PRIVATE
    SONNETCORE_STATIC_DEFINE
)

target_include_directories(gentrigrams PRIVATE ../src/core)
target_link_directories(gentrigrams PRIVATE ../src/core)

target_link_libraries(gentrigrams PRIVATE
    Qt::Core
    sonnet-core
)

add_executable(parsetrigrams
    parsetrigrams_src/parsetrigrams.cpp
)

target_link_libraries(parsetrigrams PRIVATE
    Qt::Core
)

if((QT_DEFAULT_MAJOR_VERSION GREATER 5))
    target_link_libraries(gentrigrams PRIVATE
        Qt::Core5Compat
    )
    target_link_libraries(parsetrigrams PRIVATE
        Qt::Core5Compat
    )
endif()
