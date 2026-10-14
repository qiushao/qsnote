set(QSNOTE_DIAGRAM_RUNTIME_DIR "${PROJECT_SOURCE_DIR}/build-runtime/windows" CACHE PATH
        "Prepared Windows diagram runtime (scripts/prepare-windows-runtime.ps1)")
foreach (runtime_file plantuml/plantuml.jar graphviz/bin/dot.exe)
    if (NOT EXISTS "${QSNOTE_DIAGRAM_RUNTIME_DIR}/${runtime_file}")
        message(FATAL_ERROR "Missing ${runtime_file}. Run scripts/prepare-windows-runtime.ps1 before configuring Windows builds.")
    endif ()
endforeach ()

function(qsnote_copy_diagram_runtime target)
    foreach (component plantuml graphviz)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${QSNOTE_DIAGRAM_RUNTIME_DIR}/${component}" "$<TARGET_FILE_DIR:${target}>/runtime/${component}"
                VERBATIM)
    endforeach ()
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QSNOTE_DIAGRAM_RUNTIME_DIR}/manifest.json" "$<TARGET_FILE_DIR:${target}>/runtime/manifest.json"
            VERBATIM)
endfunction()
