macro(configure_file_lazy input output)
    set(flags)
    foreach(arg ${ARGN})
        list(APPEND flags -D ${arg})
    endforeach()

    if(NOT IS_ABSOLUTE input)
        set(real_input ${CMAKE_CURRENT_SOURCE_DIR}/${input})
    else()
        set(real_input ${input})
    endif()

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -D INPUT=${real_input} -D OUTPUT=${output} ${flags} -P ${PUREDARWIN_SOURCE_DIR}/scripts/cmake/preprocess_file_script.cmake
        DEPENDS ${real_input} ${PUREDARWIN_SOURCE_DIR}/scripts/cmake/preprocess_file_script.cmake
        COMMENT "Configure ${output}" VERBATIM
    )

    # Unset our private variables before leaving macro scope
    set(flags)
    set(real_input)
endmacro()
