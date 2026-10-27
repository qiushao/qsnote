# Build the pinned C sources without a parser generator or network access.
add_library(qsnote_tree_sitter STATIC runtime/lib/src/lib.c)
target_include_directories(qsnote_tree_sitter PUBLIC runtime/lib/include PRIVATE runtime/lib/src)
target_compile_definitions(qsnote_tree_sitter PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE _BSD_SOURCE _DARWIN_C_SOURCE)
set_target_properties(qsnote_tree_sitter PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

foreach(grammar c cpp java python javascript typescript/typescript typescript/tsx bash)
    target_sources(qsnote_tree_sitter PRIVATE "${grammar}/src/parser.c")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${grammar}/src/scanner.c")
        target_sources(qsnote_tree_sitter PRIVATE "${grammar}/src/scanner.c")
        set_source_files_properties("${grammar}/src/scanner.c" PROPERTIES
                INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/${grammar}/src")
    endif()
endforeach()

if(MSVC)
    target_compile_options(qsnote_tree_sitter PRIVATE /wd4244 /wd4267)
endif()
