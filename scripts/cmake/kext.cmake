function(add_kext_bundle name)
    cmake_parse_arguments(SL "KERNEL_PRIVATE" "MACOSX_VERSION_MIN;INFO_PLIST" "" ${ARGN})

    if(SL_MACOSX_VERSION_MIN)
        add_darwin_shared_library(${name} MODULE MACOSX_VERSION_MIN ${SL_MACOSX_VERSION_MIN})
    else()
        add_darwin_shared_library(${name} MODULE)
    endif()

    set_property(TARGET ${name} PROPERTY PREFIX "")
    set_property(TARGET ${name} PROPERTY SUFFIX "")

    target_compile_definitions(${name} PRIVATE TARGET_OS_OSX KERNEL)
    target_compile_options(${name} PRIVATE -fapple-kext)
    target_link_options(${name} PRIVATE -fapple-kext)
    target_link_options(${name} PRIVATE "LINKER:-bundle")
    target_link_options(${name} PRIVATE "SHELL:-undefined dynamic_lookup")

    if(SL_KERNEL_PRIVATE)
        target_compile_definitions(${name} PRIVATE KERNEL_PRIVATE)
        target_link_libraries(${name} PRIVATE xnu_kernel_private_headers)
    endif()

    target_link_libraries(${name} PRIVATE xnu_kernel_headers AvailabilityHeaders)

    set_property(TARGET ${name} PROPERTY BUNDLE TRUE)
    set_property(TARGET ${name} PROPERTY BUNDLE_EXTENSION kext)

    if(SL_INFO_PLIST)
        get_filename_component(SL_INFO_PLIST ${SL_INFO_PLIST} ABSOLUTE)
        set_property(TARGET ${name} PROPERTY MACOSX_BUNDLE_INFO_PLIST ${SL_INFO_PLIST})
    else()
        message(SEND_ERROR "INFO_PLIST argument must be provided to add_darwin_kext()")
    endif()
endfunction()
