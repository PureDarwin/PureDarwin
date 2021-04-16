set(XNU_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(XNU_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

function(xnu_component target_name)
    cmake_parse_arguments(COMP "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})

    if(NOT COMP_SOURCES)
        message(SEND_ERROR "SOURCES must be specified to xnu_component()")
        return()
    endif()

    add_darwin_object_library(xnu.${target_name}_component)
    target_sources(xnu.${target_name}_component PRIVATE ${COMP_SOURCES})
    if(COMP_LINK_LIBRARIES)
        target_link_libraries(xnu.${target_name}_component PRIVATE ${COMP_LINK_LIBRARIES})
    endif()

    target_link_libraries(xnu.${target_name}_component PRIVATE xnu.${target_name}_headers)
    target_sources(xnu PRIVATE $<TARGET_OBJECTS:xnu.${target_name}_component>)

    target_compile_definitions(xnu.${target_name}_component PRIVATE
        APPLE KERNEL=1 KERNEL_BUILD KERNEL_PRIVATE BSD_KERNEL_PRIVATE XNU_KERNEL_PRIVATE __MACHO__=1
        volatile=__volatile XNU_KERN_EVENT_DATA_IS_VLA XNU_TARGET_OS_OSX PLATFORM_MacOSX
    )
    target_compile_options(xnu.${target_name}_component PRIVATE -DPRIVATE)
    target_link_libraries(xnu.${target_name}_component PRIVATE
        xnu.ptrauth xnu.extheaders xnu.osfmk_gssd_module xnu.bsd_sys_headers xnu.bsd_sys_types_headers
    )
    target_include_directories(xnu.${target_name}_component PRIVATE
        ${XNU_SOURCE_DIR}/iokit ${XNU_SOURCE_DIR}/libkern ${XNU_SOURCE_DIR}/osfmk
        ${XNU_SOURCE_DIR}/osfmk/libsa ${XNU_SOURCE_DIR}/pexpert ${XNU_SOURCE_DIR}
    )
endfunction()

function(install_xnu_headers)
    cmake_parse_arguments(INSTALL "" "DESTINATION;TARGET_NAME;SUBDIRECTORY" "FILES" ${ARGN})

    if(NOT INSTALL_DESTINATION)
        message(SEND_ERROR "DESTINATION must be specified to install_xnu_headers()")
        return()
    endif()

    if(NOT INSTALL_TARGET_NAME)
        message(SEND_ERROR "TARGET_NAME must be specified to install_xnu_headers()")
        return()
    endif()

    if(NOT TARGET ${INSTALL_TARGET_NAME})
        add_darwin_object_library(${INSTALL_TARGET_NAME})
    endif()

    set(install_dir "")
    set(unifdef_args)
    list(APPEND unifdef_args
        -UMACH_KERNEL_PRIVATE
        -UBSD_KERNEL_PRIVATE
        -UIOKIT_KERNEL_PRIVATE
        -ULIBKERN_KERNEL_PRIVATE
        -ULIBSA_KERNEL_PRIVATE
        -UPEXPERT_KERNEL_PRIVATE
        -UXNU_KERNEL_PRIVATE
    )

    if(INSTALL_DESTINATION STREQUAL "user")
        set(out_base ${XNU_BINARY_DIR}/include/user)
        set(install_dir usr/include)
        set(project xnu_headers)

        list(APPEND unifdef_args
            -UKERNEL_PRIVATE
            -UKERNEL
            -UPRIVATE
            -UDRIVERKIT
            -D_OPEN_SOURCE_
            -D__OPEN_SOURCE__
        )
    elseif(INSTALL_DESTINATION STREQUAL "user_private")
        set(out_base ${XNU_BINARY_DIR}/include/user_private)
        set(project xnu_private_headers)

        list(APPEND unifdef_args
            -UKERNEL_PRIVATE
            -UKERNEL
            -DPRIVATE
            -UDRIVERKIT
            -U_OPEN_SOURCE_
            -U__OPEN_SOURCE__
        )
    elseif(INSTALL_DESTINATION STREQUAL "kernel")
        set(out_base ${XNU_BINARY_DIR}/include/kernel)
        set(install_dir System/Library/Frameworks/Kernel.framework/Versions/B/Headers)
        set(project xnu_kernel_headers)

        list(APPEND unifdef_args
            -UKERNEL_PRIVATE -DKERNEL -UPRIVATE -UDRIVERKIT -D_OPEN_SOURCE_ -D__OPEN_SOURCE__
        )
    elseif(INSTALL_DESTINATION STREQUAL "kernel_private")
        set(out_base ${XNU_BINARY_DIR}/include/kernel_private)
        set(project xnu_kernel_private_headers)

        list(APPEND unifdef_args
            -DKERNEL_PRIVATE -DKERNEL -DPRIVATE -UDRIVERKIT -U_OPEN_SOURCE_ -U__OPEN_SOURCE__
        )
    else()
        message(SEND_ERROR "Invalid DESTINATION value ${INSTALL_DESTINATION}")
        return()
    endif()

    foreach(file IN LISTS INSTALL_FILES)
        get_filename_component(file_abs ${file} ABSOLUTE)
        get_filename_component(filename ${file} NAME)
        add_custom_command(
            OUTPUT ${out_base}/${INSTALL_SUBDIRECTORY}/${filename}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_base}/${INSTALL_SUBDIRECTORY}
            COMMAND host_unifdef ${unifdef_args} -o ${out_base}/${INSTALL_SUBDIRECTORY}/${filename} ${file_abs}
            DEPENDS ${file_abs} host_unifdef
            COMMENT "Copy ${INSTALL_SUBDIRECTORY}/${filename}" VERBATIM
        )
        target_sources(${INSTALL_TARGET_NAME} PRIVATE ${out_base}/${INSTALL_SUBDIRECTORY}/${filename})

        if(NOT install_dir STREQUAL "")
            install(FILES ${out_base}/${INSTALL_SUBDIRECTORY}/${filename} DESTINATION ${install_dir} COMPONENT DeveloperTools)
        endif()
    endforeach()

    add_dependencies(${project} ${INSTALL_TARGET_NAME})
endfunction()
