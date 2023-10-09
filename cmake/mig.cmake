function(mig filename)
    cmake_parse_arguments(MIG "NOVOUCHERS;NODEFINES" "INCLUDES_FROM_TARGET;USER_HEADER;USER_SOURCE;SERVER_HEADER;SERVER_SOURCE;ARCH" "" ${ARGN})

    set(MIG_DEPS)
    if(MIG_USER_HEADER)
        list(APPEND MIG_DEPS ${MIG_USER_HEADER})
    else()
        set(MIG_USER_HEADER /dev/null)
    endif()
    if(MIG_USER_SOURCE)
        list(APPEND MIG_DEPS ${MIG_USER_SOURCE})
    else()
        set(MIG_USER_SOURCE /dev/null)
    endif()
    if(MIG_SERVER_HEADER)
        list(APPEND MIG_DEPS ${MIG_SERVER_HEADER})
    else()
        set(MIG_SERVER_HEADER /dev/null)
    endif()
    if(MIG_SERVER_SOURCE)
        list(APPEND MIG_DEPS ${MIG_SERVER_SOURCE})
    else()
        set(MIG_SERVER_SOURCE /dev/null)
    endif()

    if(MIG_USER_HEADER STREQUAL "/dev/null" AND MIG_USER_SOURCE STREQUAL "/dev/null" AND MIG_SERVER_HEADER STREQUAL "/dev/null" AND MIG_SERVER_SOURCE STREQUAL "/dev/null")
        message(SEND_ERROR "At least one output must be specified")
        return()
    endif()

    set(MIG_FLAGS)

    if(MIG_INCLUDES_FROM_TARGET)
        set(incs $<TARGET_PROPERTY:${MIG_INCLUDES_FROM_TARGET},INCLUDE_DIRECTORIES>)
        list(APPEND MIG_FLAGS $<$<BOOL:${incs}>:-I$<JOIN:${incs},$<SEMICOLON>-I>>)

        if(NOT MIG_NODEFINES)
            set(defs $<TARGET_PROPERTY:${MIG_INCLUDES_FROM_TARGET},COMPILE_DEFINITIONS>)
            list(APPEND MIG_FLAGS $<$<BOOL:${defs}>:-D$<JOIN:${defs},$<SEMICOLON>-D>>)
        endif()

        target_get_library_dependencies(${MIG_INCLUDES_FROM_TARGET} target_dependencies)
        foreach(dep IN LISTS target_dependencies)
            set(incs $<TARGET_PROPERTY:${dep},INTERFACE_INCLUDE_DIRECTORIES>)
            list(APPEND MIG_FLAGS $<$<BOOL:${incs}>:-I$<JOIN:${incs},$<SEMICOLON>-I>>)

            if(NOT MIG_NODEFINES)
                set(defs $<TARGET_PROPERTY:${dep},INTERFACE_COMPILE_DEFINITIONS>)
                list(APPEND MIG_FLAGS $<$<BOOL:${defs}>:-D$<JOIN:${defs},$<SEMICOLON>-D>>)
            endif()
        endforeach()
    else()
        message(WARNING "No TARGET specified in mig() call; the MiG preprocessor will probably fail")
    endif()

    if(MIG_NOVOUCHERS)
        list(APPEND MIG_FLAGS -novouchers)
    endif()

    if(NOT MIG_ARCH)
        set(MIG_ARCH i386)
    endif()

    get_filename_component(basename ${filename} NAME_WE)
    get_filename_component(filename_abs ${filename} ABSOLUTE)
    add_custom_command(OUTPUT ${MIG_DEPS}
        COMMAND ${CMAKE_COMMAND} -E env MIGCOM=$<TARGET_FILE:migcom> ${PUREDARWIN_SOURCE_DIR}/tools/mig/mig.sh -arch ${MIG_ARCH}
            -user ${MIG_USER_SOURCE} -header ${MIG_USER_HEADER} -server ${MIG_SERVER_SOURCE}
            -sheader ${MIG_SERVER_HEADER} ${MIG_FLAGS} ${filename_abs}
        DEPENDS migcom
        COMMENT "MiG ${filename}" VERBATIM COMMAND_EXPAND_LISTS
    )
endfunction()
