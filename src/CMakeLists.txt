find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_add_translation(QSNOTE_QM_FILES translations/qsnote_en_US.ts)
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
execute_process(COMMAND "${QT_QMAKE_EXECUTABLE}" -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE QT_TRANSLATIONS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
configure_file(resources/translations.qrc.in translations.qrc @ONLY)
add_library(qsnote_i18n OBJECT
        core/ApplicationLanguage.cpp
        ${QSNOTE_QM_FILES}
        ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc
        ../third_party/qsnote-translations.qrc
)
target_include_directories(qsnote_i18n PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qsnote_i18n PUBLIC Qt6::Core)

add_executable(qsnote WIN32 MACOSX_BUNDLE
        main.cpp
        ui/MainWindow.cpp
        ui/MainWindow.h
        ui/PostPropertiesDialog.cpp
        ui/PostPropertiesDialog.h
        ui/OverlayDialog.cpp
        ui/OverlayDialog.h
        ui/SettingsDialog.cpp
        ui/SettingsDialog.h
        ui/SiteConfigDialog.cpp
        ui/SiteConfigDialog.h
        ui/MarkdownEditor.cpp
        ui/MarkdownEditor.h
        ui/InplacePreview.cpp
        ui/InplacePreview.h
        ui/MarkdownPreview.cpp
        ui/MarkdownPreview.h
        ui/FlowLayout.cpp
        ui/FlowLayout.h
        core/UpdateChecker.cpp
        core/UpdateChecker.h
        core/NotebookStore.cpp
        core/NotebookCategories.cpp
        core/NotebookCategories.h
        core/ChapterOperations.cpp
        core/ChapterOperations.h
        core/AtomicFile.cpp
        core/AtomicFile.h
        core/ArticleRenderer.cpp
        core/MathStylesheet.cpp
        core/MathStylesheet.h
        core/EbookExporter.cpp
        core/EbookExporter.h
        core/PlantUmlRenderer.cpp
        core/PlantUmlRenderer.h
        core/DiagramRuntime.cpp
        core/DiagramRuntime.h
        core/StaticSite.cpp
        core/SiteGenerator.cpp
        core/SitePublisher.cpp
        core/SitePublisher.h
        core/StaticSite.h
        core/NotebookStore.h
        resources/ui.qrc
        resources/site.qrc
        ../third_party/vtextedit/demo/data/syntax/syntax.qrc
        ../third_party/vnote-web.qrc
)

target_include_directories(qsnote PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qsnote PRIVATE qsnote_i18n Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Network Qt6::Xml VTextEdit)
target_compile_definitions(qsnote PRIVATE APP_VERSION="${APP_VERSION}")

if (WIN32)
    add_custom_command(TARGET qsnote POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:VTextEdit> $<TARGET_FILE_DIR:qsnote>
            VERBATIM)
    include(${PROJECT_SOURCE_DIR}/cmake/WindowsDiagramRuntime.cmake)
    qsnote_copy_diagram_runtime(qsnote)
    install(DIRECTORY "${QSNOTE_DIAGRAM_RUNTIME_DIR}/plantuml" "${QSNOTE_DIAGRAM_RUNTIME_DIR}/graphviz"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/runtime")
    install(FILES "${QSNOTE_DIAGRAM_RUNTIME_DIR}/manifest.json" DESTINATION "${CMAKE_INSTALL_BINDIR}/runtime")
endif ()

set_target_properties(qsnote PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "qsnote"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.qiushao.qsnote"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
)

if (UNIX AND NOT APPLE)
    set_target_properties(qsnote PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
elseif (APPLE)
    set_target_properties(qsnote PROPERTIES
            INSTALL_RPATH "@executable_path/../Frameworks"
            MACOSX_BUNDLE_ICON_FILE "qsnote.icns")
    set(app_icon "${CMAKE_CURRENT_BINARY_DIR}/qsnote.icns")
    add_custom_command(OUTPUT "${app_icon}"
            COMMAND bash "${PROJECT_SOURCE_DIR}/scripts/macos-create-icon.sh" "${app_icon}"
            DEPENDS resources/icons/qsnote.png "${PROJECT_SOURCE_DIR}/scripts/macos-create-icon.sh"
            VERBATIM)
    set_source_files_properties("${app_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(qsnote PRIVATE "${app_icon}")
endif ()

install(TARGETS qsnote
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES resources/qsnote.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/qsnote.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/UpdateIconCache.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/UpdateIconCache.cmake" @ONLY)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/UpdateIconCache.cmake")
endif ()
