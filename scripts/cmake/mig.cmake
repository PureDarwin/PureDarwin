function(process_mig_source filename)
    cmake_parse_arguments(MIG "NOVOUCHERS" "ARCH;SERVER_HEADER;CLIENT_HEADER;SERVER_SOURCE;CLIENT_SOURCE;TARGET" "" ${ARGN})

    if((NOT MIG_SERVER_HEADER) AND (NOT MIG_CLIENT_HEADER) AND (NOT MIG_SERVER_SOURCE) AND (NOT MIG_CLIENT_SOURCE))
        message(SEND_ERROR "You must specify at least one MiG output")
        return()
    endif()


    set(MIG_FLAGS)
    if(MIG_TARGET)
        get_property(_defs TARGET ${MIG_TARGET} PROPERTY COMPILE_DEFINITIONS)
        get_property(_incs TARGET ${MIG_TARGET} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)

        foreach(def ${_defs})
            list(APPEND MIG_FLAGS "-D${def}")
        endforeach()
        foreach(inc ${_incs})
            list(APPEND MIG_FLAGS "-I${inc}")
        endforeach()
    endif()
    if(MIG_NOVOUCHERS)
        list(APPEND MIG_FLAGS "-novouchers")
    endif()

    if(NOT MIG_ARCH)
        set(MIG_ARCH i386)
    endif()

    set(MIG_OUTPUTS)
    if(MIG_SERVER_HEADER)
        list(APPEND MIG_FLAGS "-sheader" ${MIG_SERVER_HEADER})
        list(APPEND MIG_OUTPUTS ${MIG_SERVER_HEADER})
    endif()
    if(MIG_CLIENT_HEADER)
        list(APPEND MIG_FLAGS "-header" ${MIG_CLIENT_HEADER})
        list(APPEND MIG_OUTPUTS ${MIG_CLIENT_HEADER})
    endif()
    if(MIG_SERVER_SOURCE)
        list(APPEND MIG_FLAGS "-server" ${MIG_SERVER_SOURCE})
        list(APPEND MIG_OUTPUTS ${MIG_SERVER_SOURCE})
    endif()
    if(MIG_CLIENT_SOURCE)
        list(APPEND MIG_FLAGS "-user" ${MIG_CLIENT_SOURCE})
        list(APPEND MIG_OUTPUTS ${MIG_CLIENT_SOURCE})
    endif()

    add_custom_command(
        OUTPUT ${MIG_OUTPUTS}
        COMMAND
            ${CMAKE_COMMAND} -E env MIGCOM=$<TARGET_FILE:migcom> $<TARGET_FILE:mig> -arch ${MIG_ARCH}
                ${MIG_FLAGS} ${filename}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS mig migcom ${filename}
        COMMENT "MiG ${filename}" VERBATIM
    )
endfunction()
