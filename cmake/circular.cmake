# This function has been taken and modified from Darling.
# https://github.com/darlinghq/darling/blob/master/cmake/darling_lib.cmake#L116
# SPDX-License-Identifier: GPL-3

function(add_darwin_circular_library name)
    cmake_parse_arguments(CIRCULAR "" "INSTALL_NAME" "OBJECT_LIBRARIES;SIBLINGS;STRONG_SIBLINGS;DEPENDENCIES;UPWARD;STRONG_DEPENDENCIES" ${ARGN})

    if(NOT CIRCULAR_OBJECT_LIBRARIES)
        message(FATAL_ERROR "Need to specify at least one value under OBJECT_LIBRARIES")
    endif()

    set(firstpass_name "${name}_firstpass")
    add_darwin_shared_library(${firstpass_name} INSTALL_NAME ${CIRCULAR_INSTALL_NAME})
    set_property(TARGET ${firstpass_name} PROPERTY OUTPUT_NAME ${name})
    set_property(TARGET ${firstpass_name} PROPERTY SUFFIX .firstpass_dylib)

    foreach(lib ${CIRCULAR_OBJECT_LIBRARIES})
        target_sources(${firstpass_name} PRIVATE $<TARGET_OBJECTS:${lib}>)
    endforeach()

    target_link_options(${firstpass_name} PRIVATE "LINKER:-flat_namespace" "LINKER:-undefined suppress")

    foreach(dep ${CIRCULAR_STRONG_SIBLINGS})
        target_link_libraries(${firstpass_name} PRIVATE "${dep}_firstpass")
    endforeach()

    target_link_libraries(${firstpass_name} PRIVATE ${CIRCULAR_STRONG_DEPENDENCIES})

    add_darwin_shared_library(${name} INSTALL_NAME ${CIRCULAR_INSTALL_NAME})
    foreach(lib ${CIRCULAR_OBJECT_LIBRARIES})
        target_sources(${name} PRIVATE $<TARGET_OBJECTS:${lib}>)
    endforeach()

    foreach(dep ${CIRCULAR_SIBLINGS})
        target_link_libraries(${name} PRIVATE "${dep}_firstpass")
        add_dependencies(${name} "${dep}_firstpass")
    endforeach()
    foreach(dep ${CIRCULAR_UPWARD})
        target_link_options(${name} PRIVATE "LINKER:-upward_library $<TARGET_FILE:${dep}>")
        add_dependencies(${name} "${dep}_firstpass")
    endforeach()

    target_link_libraries(${name} PRIVATE ${CIRCULAR_DEPENDENCIES})
    target_link_libraries(${name} PRIVATE ${CIRCULAR_STRONG_DEPENDENCIES})
endfunction()
