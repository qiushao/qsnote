add_executable(qsnote WIN32 MACOSX_BUNDLE
        main.cpp
        ui/MainWindow.cpp
        ui/MainWindow.h
        ui/FlowLayout.cpp
        ui/FlowLayout.h
        core/NotebookStore.cpp
        core/NotebookStore.h
        resources/ui.qrc
)

target_include_directories(qsnote PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qsnote PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(qsnote PRIVATE APP_VERSION="${APP_VERSION}")

set_target_properties(qsnote PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "qsnote"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.qiushao.qsnote"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
)

install(TARGETS qsnote
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES resources/qsnote.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif ()
