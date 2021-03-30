macro(preprocess_file input output)
    set(flags)
    foreach(arg IN LISTS ARGN)
        list(APPEND flags -D ${arg})
    endforeach()

    if(NOT IS_ABSOLUTE input)
        set(input ${input} ${CMAKE_CURRENT_SOURCE_DIR}/${input})
    endif()

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -P ${PUREDARWIN_SOURCE_DIR}/scripts/cmake/preprocess_file_script.cmake -D INPUT=${input} -D OUTPUT=${output} ${flags}
        DEPENDS ${input}
        COMMENT "Configure ${output}" VERBATIM
    )

    # Unset our private variables before leaving macro scope
    set(flags)
endmacro()
