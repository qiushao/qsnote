add_executable(qsnote WIN32 MACOSX_BUNDLE
        main.cpp
        ui/MainWindow.cpp
        ui/MainWindow.h
        ui/MarkdownEditor.cpp
        ui/MarkdownEditor.h
        ui/MarkdownPreview.cpp
        ui/MarkdownPreview.h
        ui/FlowLayout.cpp
        ui/FlowLayout.h
        core/NotebookStore.cpp
        core/HexoSite.cpp
        core/HexoSite.h
        core/NotebookStore.h
        resources/ui.qrc
        resources/hexo.qrc
        ../third_party/vtextedit/demo/data/syntax/syntax.qrc
        ../third_party/qsnote-translations.qrc
        ../third_party/vnote-web.qrc
)

target_include_directories(qsnote PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qsnote PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Network VTextEdit)
target_compile_definitions(qsnote PRIVATE APP_VERSION="${APP_VERSION}")

set_target_properties(qsnote PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "qsnote"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.qiushao.qsnote"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
)

if (UNIX AND NOT APPLE)
    set_target_properties(qsnote PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif ()

install(TARGETS qsnote
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES resources/qsnote.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif ()
