cmake_minimum_required(VERSION 3.18)
project(qsnote_encoding_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Core Core5Compat)
if (MSVC)
    add_compile_options(/utf-8)
endif ()
enable_testing()
add_executable(text_encoding_test TextEncodingTest.cpp ../src/core/TextEncoding.cpp)
target_include_directories(text_encoding_test PRIVATE ../src)
target_link_libraries(text_encoding_test PRIVATE Qt6::Core Qt6::Core5Compat)
add_test(NAME text_encoding COMMAND text_encoding_test)

option(QSNOTE_TEST_SOURCE_HIGHLIGHTING "Build source highlighting regression tests" ON)
if (QSNOTE_TEST_SOURCE_HIGHLIGHTING)
    enable_language(C)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(QT_DEFAULT_MAJOR_VERSION 6)
    find_package(Qt6 REQUIRED COMPONENTS Gui Widgets Test Network)
    add_subdirectory(../third_party/tree-sitter tree-sitter)
    add_subdirectory(../third_party/vtextedit/libs/syntax-highlighting syntax-highlighting)
    add_executable(source_highlighter_test
            SourceHighlighterTest.cpp
            ../src/ui/SourceHighlighter.cpp
            ../src/core/SourceLanguage.cpp
            ../src/resources/source-syntax.qrc
            ../third_party/vtextedit/demo/data/syntax/syntax.qrc)
    target_include_directories(source_highlighter_test PRIVATE ../src)
    target_link_libraries(source_highlighter_test PRIVATE Qt6::Widgets Qt6::Test VSyntaxHighlighting qsnote_tree_sitter)
    add_test(NAME source_highlighter COMMAND source_highlighter_test)
    set_tests_properties(source_highlighter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif ()
