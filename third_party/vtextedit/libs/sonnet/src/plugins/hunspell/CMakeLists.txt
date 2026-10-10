cmake_minimum_required(VERSION 3.16)
project(sonnet-hunspell VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt version to use (5 or 6), defaults to 6")
find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core)

if ((QT_DEFAULT_MAJOR_VERSION GREATER 5))
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

add_library(sonnet-hunspell STATIC
    config-hunspell.h
    hunspellclient.cpp hunspellclient.h
    hunspelldebug.cpp hunspelldebug.h
    hunspelldict.cpp hunspelldict.h
)
target_compile_definitions(sonnet-hunspell PUBLIC
    INSTALLATION_PLUGIN_PATH=""
    SONNET_STATIC
)

target_include_directories(sonnet-hunspell PUBLIC ../../core)
target_link_directories(sonnet-hunspell PUBLIC ../../core)

target_link_libraries(sonnet-hunspell PUBLIC
    Qt::Core
    sonnet-core
    Hunspell
)
