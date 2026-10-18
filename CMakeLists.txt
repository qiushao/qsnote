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

if (DEFINED ENV{Qt6_DIR})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
endif ()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets WebChannel Xml)
if (NOT TARGET Qt6::GuiPrivate)
    find_package(Qt6GuiPrivate REQUIRED)
endif ()

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
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "plantuml, graphviz, default-jre-headless")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    include(CPack)
endif ()

option(QSNOTE_BUILD_TESTS "Build UI interaction tests" OFF)
if (QSNOTE_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(qsnote_site_tests
            tests/StaticSiteTest.cpp
            src/core/NotebookStore.cpp
            src/core/StaticSite.cpp
            src/core/SiteGenerator.cpp
            src/core/ArticleRenderer.cpp
            src/resources/site.qrc
            src/core/PlantUmlRenderer.cpp
            src/resources/ui.qrc
            src/core/DiagramRuntime.cpp
            third_party/vnote-web.qrc
    )
    target_include_directories(qsnote_site_tests PRIVATE src)
    target_link_libraries(qsnote_site_tests PRIVATE Qt6::Core Qt6::Test Qt6::WebEngineWidgets Qt6::Xml VTextEdit)
    add_test(NAME qsnote_site COMMAND qsnote_site_tests)
    set_tests_properties(qsnote_site PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")
    add_executable(qsnote_ebook_tests
            tests/EbookExporterTest.cpp
            src/core/EbookExporter.cpp
            src/core/PlantUmlRenderer.cpp
            src/core/DiagramRuntime.cpp
            src/core/NotebookStore.cpp
            src/core/StaticSite.cpp
            src/core/SiteGenerator.cpp
            src/core/ArticleRenderer.cpp
            src/resources/site.qrc
            third_party/vnote-web.qrc
            src/resources/ui.qrc
    )
    target_include_directories(qsnote_ebook_tests PRIVATE src)
    target_link_libraries(qsnote_ebook_tests PRIVATE Qt6::Widgets Qt6::GuiPrivate Qt6::Test Qt6::WebEngineWidgets Qt6::Network Qt6::Xml VTextEdit)
    add_test(NAME qsnote_ebook COMMAND qsnote_ebook_tests)
    set_tests_properties(qsnote_ebook PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu")
    add_executable(qsnote_publish_tests
            tests/SitePublisherTest.cpp
            src/core/SitePublisher.cpp
            src/core/StaticSite.cpp
            src/core/SiteGenerator.cpp
            src/core/ArticleRenderer.cpp
            src/core/PlantUmlRenderer.cpp
            src/core/DiagramRuntime.cpp
            src/resources/ui.qrc
            third_party/vnote-web.qrc
    )
    target_include_directories(qsnote_publish_tests PRIVATE src)
    target_link_libraries(qsnote_publish_tests PRIVATE Qt6::Core Qt6::Test Qt6::WebEngineWidgets Qt6::Xml)
    add_test(NAME qsnote_publish COMMAND qsnote_publish_tests)
    add_executable(qsnote_ui_tests
            src/core/UpdateChecker.cpp
            src/core/SitePublisher.cpp
            tests/UiTest.cpp
            src/core/EbookExporter.cpp
            src/core/PlantUmlRenderer.cpp
            src/core/DiagramRuntime.cpp
            src/ui/MainWindow.cpp
            src/ui/MarkdownEditor.cpp
            src/ui/InplacePreview.cpp
            src/ui/MarkdownPreview.cpp
            src/ui/FlowLayout.cpp
            src/core/NotebookStore.cpp
            src/core/StaticSite.cpp
            src/core/SiteGenerator.cpp
            src/core/ArticleRenderer.cpp
            src/resources/ui.qrc
            src/resources/site.qrc
            third_party/vtextedit/demo/data/syntax/syntax.qrc
            third_party/vnote-web.qrc
    )
    target_include_directories(qsnote_ui_tests PRIVATE src)
    target_link_libraries(qsnote_ui_tests PRIVATE qsnote_i18n Qt6::Widgets Qt6::GuiPrivate Qt6::Test Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Network VTextEdit Qt6::Xml)
    add_test(NAME qsnote_ui COMMAND qsnote_ui_tests)
    set_tests_properties(qsnote_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-data")
    add_executable(qsnote_diagram_tests
            tests/DiagramRuntimeTest.cpp
            src/core/DiagramRuntime.cpp
            src/core/PlantUmlRenderer.cpp
    )
    target_include_directories(qsnote_diagram_tests PRIVATE src)
    target_link_libraries(qsnote_diagram_tests PRIVATE Qt6::Gui Qt6::Test)
    set_target_properties(qsnote_diagram_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/diagram-tests")
    if (WIN32)
        qsnote_copy_diagram_runtime(qsnote_diagram_tests)
    endif ()
    add_test(NAME qsnote_diagram COMMAND qsnote_diagram_tests)
endif ()
