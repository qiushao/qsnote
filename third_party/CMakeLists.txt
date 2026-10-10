# Keep upstream code and its dependencies in this directory. No network access
# or submodule checkout is needed when configuring or building QSNote.
set(CMAKE_CXX_CLANG_TIDY "")
set(CMAKE_C_CLANG_TIDY "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(QT_DEFAULT_MAJOR_VERSION 6)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(vtextedit/libs EXCLUDE_FROM_ALL)
add_subdirectory(vtextedit/src)

if (WIN32)
    set(QSNOTE_LICENSE_DIR "${CMAKE_INSTALL_BINDIR}/licenses")
else ()
    set(QSNOTE_LICENSE_DIR "${CMAKE_INSTALL_DATADIR}/licenses/qsnote")
endif ()
install(FILES README.md DESTINATION ${QSNOTE_LICENSE_DIR})
install(FILES vtextedit/COPYING.LESSER DESTINATION ${QSNOTE_LICENSE_DIR}/vtextedit)
install(FILES vtextedit/libs/cmark/COPYING DESTINATION ${QSNOTE_LICENSE_DIR}/cmark)
install(FILES vtextedit/libs/syntax-highlighting/COPYING DESTINATION ${QSNOTE_LICENSE_DIR}/syntax-highlighting)
install(FILES vtextedit/libs/hunspell/COPYING vtextedit/libs/hunspell/COPYING.LESSER vtextedit/libs/hunspell/COPYING.MPL
        DESTINATION ${QSNOTE_LICENSE_DIR}/hunspell)
install(DIRECTORY vtextedit/libs/sonnet/LICENSES/ DESTINATION ${QSNOTE_LICENSE_DIR}/sonnet)
