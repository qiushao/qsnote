cmake_minimum_required(VERSION 3.18)
project(qsnote VERSION 1.0.0 LANGUAGES C CXX)

set(APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Application version")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/utf-8)
endif ()

if (WIN32 AND DEFINED ENV{Qt6_DIR})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
endif ()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebChannel)

option(QSNOTE_CLANG_TIDY_ENABLE "Run clang-tidy" ON)
if (QSNOTE_CLANG_TIDY_ENABLE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
endif ()

include(GNUInstallDirs)
add_subdirectory(third_party)
add_subdirectory(src)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "qsnote")
    set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "QSNote Qt desktop application")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "qiushao")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    include(CPack)
endif ()

option(QSNOTE_BUILD_TESTS "Build UI interaction tests" OFF)
if (QSNOTE_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(qsnote_ui_tests
            tests/UiTest.cpp
            src/ui/MainWindow.cpp
            src/ui/MarkdownEditor.cpp
            src/ui/MarkdownPreview.cpp
            src/ui/FlowLayout.cpp
            src/core/NotebookStore.cpp
            src/resources/ui.qrc
            third_party/vtextedit/demo/data/syntax/syntax.qrc
            third_party/vnote-web.qrc
    )
    target_include_directories(qsnote_ui_tests PRIVATE src)
    target_link_libraries(qsnote_ui_tests PRIVATE Qt6::Widgets Qt6::Test Qt6::WebEngineWidgets Qt6::WebChannel VTextEdit)
    add_test(NAME qsnote_ui COMMAND qsnote_ui_tests)
    set_tests_properties(qsnote_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-data")
endif ()
