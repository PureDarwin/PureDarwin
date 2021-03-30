macro(preprocess_file input output)
    set(flags)
    foreach(arg ${ARGN})
        list(APPEND flags -D ${arg})
    endforeach()

    if(NOT IS_ABSOLUTE input)
        set(input2 ${CMAKE_CURRENT_SOURCE_DIR}/${input})
    else()
        set(input2 ${input})
    endif()

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -D INPUT=${input2} -D OUTPUT=${output} ${flags} -P ${PUREDARWIN_SOURCE_DIR}/scripts/cmake/preprocess_file_script.cmake
        DEPENDS ${input2} ${PUREDARWIN_SOURCE_DIR}/scripts/cmake/preprocess_file_script.cmake
        COMMENT "Configure ${output}" VERBATIM
    )

    # Unset our private variables before leaving macro scope
    set(flags)
    set(input2)
endmacro()
