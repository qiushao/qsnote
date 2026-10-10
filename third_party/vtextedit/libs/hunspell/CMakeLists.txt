cmake_minimum_required(VERSION 3.16)
project(Hunspell VERSION 1.0 LANGUAGES C CXX)

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

add_library(Hunspell STATIC
    src/hunspell/affentry.cxx src/hunspell/affentry.hxx
    src/hunspell/affixmgr.cxx src/hunspell/affixmgr.hxx
    src/hunspell/atypes.hxx
    src/hunspell/baseaffix.hxx
    src/hunspell/csutil.cxx src/hunspell/csutil.hxx
    src/hunspell/filemgr.cxx src/hunspell/filemgr.hxx
    src/hunspell/hashmgr.cxx src/hunspell/hashmgr.hxx
    src/hunspell/htypes.hxx
    src/hunspell/hunspell.cxx src/hunspell/hunspell.h src/hunspell/hunspell.hxx
    src/hunspell/hunvisapi.h
    src/hunspell/hunvisapi.h.in
    src/hunspell/hunzip.cxx src/hunspell/hunzip.hxx
    src/hunspell/langnum.hxx
    src/hunspell/phonet.cxx src/hunspell/phonet.hxx
    src/hunspell/replist.cxx src/hunspell/replist.hxx
    src/hunspell/suggestmgr.cxx src/hunspell/suggestmgr.hxx
    src/hunspell/utf_info.hxx
    src/hunspell/w_char.hxx
    src/parsers/firstparser.cxx src/parsers/firstparser.hxx
    src/parsers/htmlparser.cxx src/parsers/htmlparser.hxx
    src/parsers/latexparser.cxx src/parsers/latexparser.hxx
    src/parsers/manparser.cxx src/parsers/manparser.hxx
    src/parsers/odfparser.cxx src/parsers/odfparser.hxx
    src/parsers/testparser.cxx
    src/parsers/textparser.cxx src/parsers/textparser.hxx
    src/parsers/xmlparser.cxx src/parsers/xmlparser.hxx
)
target_include_directories(Hunspell PUBLIC
    src/hunspell
    src/parsers
)

target_compile_definitions(Hunspell PUBLIC
    HUNSPELL_STATIC
)

target_link_libraries(Hunspell PUBLIC
    Qt::Core
)
