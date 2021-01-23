function(target_add_mig_sources target filename)
    cmake_parse_arguments(MIG "SERVER;CLIENT" "ARCH" "" ${ARGN})
    if(MIG_SERVER AND MIG_CLIENT)
        message(SEND_ERROR "Only one of SERVER and CLIENT must be specified")
    endif()

    if(NOT MIG_USER_SOURCE_SUFFIX)
        set(MIG_USER_SOURCE_SUFFIX User.c)
    endif()
    if(NOT MIG_USER_HEADER_SUFFIX)
        set(MIG_USER_HEADER_SUFFIX User.h)
    endif()
    if(NOT MIG_SERVER_SOURCE_SUFFIX)
        set(MIG_USER_SOURCE_SUFFIX Server.c)
    endif()
    if(NOT MIG_USER_HEADER_SUFFIX)
        set(MIG_USER_HEADER_SUFFIX Server.h)
    endif()

    get_target_property(_defs ${target} COMPILE_DEFINITIONS)
    get_target_property(_incs ${target} INCLUDE_DIRECTORIES)

    set(MIG_FLAGS)
    foreach(def ${_defs})
        list(APPEND MIG_FLAGS "-D${def}")
    endforeach()
    foreach(inc ${_incs})
        list(APPEND MIG_FLAGS "-I${inc}")
    endforeach()

    if(NOT MIG_ARCH)
        set(MIG_ARCH i386)
    endif()

    get_filename_component(basename ${filename} NAME_WE)
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_USER_SOURCE_SUFFIX}
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_USER_HEADER_SUFFIX}
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_SERVER_SOURCE_SUFFIX}
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_SERVER_HEADER_SUFFIX}
        COMMAND
            ${CMAKE_COMMAND} -E env MIGCOM=$<TARGET_FILE:migcom> $<TARGET_FILE:mig> -arch ${MIG_ARCH}
                -user ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_USER_SOURCE_SUFFIX}
                -header ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_USER_HEADER_SUFFIX}
                -server ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_SERVER_SOURCE_SUFFIX}
                -sheader ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_SERVER_HEADER_SUFFIX}
                ${MIG_FLAGS} ${filename}
        DEPENDS mig migcom
        COMMENT "Mig ${filename}" VERBATIM
    )

    if(MIG_SERVER)
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_SERVER_SOURCE_SUFFIX}
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_SERVER_HEADER_SUFFIX}
        )
    elseif(MIG_CLIENT)
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_USER_SOURCE_SUFFIX}
            ${CMAKE_CURRENT_BINARY_DIR}/${basename}${MIG_USER_HEADER_SUFFIX}
        )
    endif()
endfunction()
