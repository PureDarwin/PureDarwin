define_property(TARGET PROPERTY INSTALL_NAME_LEAF
    BRIEF_DOCS "Filename to be used in the install name of a shared library"
    FULL_DOCS "The filename to be appended to the INSTALL_NAME_DIR parameter to add_darwin_shared_library(). Defaults to \"$<TARGET_FILE_NAME:{tgt}>\"."
)

function(add_darwin_executable name)
    cmake_parse_arguments(SL "NO_STANDARD_LIBRARIES" "MACOSX_VERSION_MIN" "" ${ARGN})

    add_executable(${name})
    add_dependencies(${name} darwin_ld)
    target_compile_definitions(${name} PRIVATE __PUREDARWIN__)
    target_link_options(${name} PRIVATE -fuse-ld=$<TARGET_FILE:darwin_ld>)
    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
    target_link_options(${name} PRIVATE -nostdlib)

    # TODO: Handle SL_NO_STANDARD_LIBRARIES here, once the libraries have been added to the build.

    if(SL_MACOSX_VERSION_MIN)
        target_compile_options(${name} PRIVATE -mmacosx-version-min=${SL_MACOSX_VERSION_MIN})
        target_link_options(${name} PRIVATE -mmacosx-version-min=${SL_MACOSX_VERSION_MIN})
    endif()
endfunction()

function(add_darwin_static_library name)
    add_library(${name} STATIC)
    add_dependencies(${name} darwin_libtool)
    target_compile_definitions(${name} PRIVATE __PUREDARWIN__)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()

    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
endfunction()

function(add_darwin_shared_library name)
    cmake_parse_arguments(SL "MODULE" "MACOSX_VERSION_MIN;INSTALL_NAME_DIR" "RPATHS" ${ARGN})

    if(SL_MODULE)
        add_library(${name} MODULE)
    else()
        add_library(${name} SHARED)
    endif()

    add_dependencies(${name} darwin_ld)
    target_link_options(${name} PRIVATE -fuse-ld=$<TARGET_FILE:darwin_ld>)
    target_compile_definitions(${name} PRIVATE __PUREDARWIN__)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()

    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
    target_link_options(${name} PRIVATE -nostdlib)
    target_link_options(${name} PRIVATE "LINKER:-not_for_dyld_shared_cache")

    if(SL_MACOSX_VERSION_MIN)
        target_compile_options(${name} PRIVATE -mmacosx-version-min=${SL_MACOSX_VERSION_MIN})
        target_link_options(${name} PRIVATE -mmacosx-version-min=${SL_MACOSX_VERSION_MIN})
    endif()

    if(SL_INSTALL_NAME_DIR)
        set_property(TARGET ${name} PROPERTY NO_SONAME TRUE)
        set_property(TARGET ${name} PROPERTY INSTALL_NAME_DIR ${SL_INSTALL_NAME_DIR})
        set_property(TARGET ${name} PROPERTY INSTALL_NAME_LEAF $<TARGET_FILE_NAME:${name}>)
        target_link_options(${name} PRIVATE -install_name ${SL_INSTALL_NAME_DIR}/$<GENEX_EVAL:$<TARGET_PROPERTY:${name},INSTALL_NAME_LEAF>>)
    elseif(NOT SL_MODULE)
        message(WARNING "Shared library target ${name} should have INSTALL_NAME_DIR defined")
    endif()

    foreach(rpath IN LISTS SL_RPATHS)
        target_link_options(${name} PRIVATE "SHELL:-rpath ${rpath}")
    endforeach()
endfunction()

function(add_darwin_object_library name)
    add_library(${name} OBJECT)
    set_property(TARGET ${name} PROPERTY LINKER_LANGUAGE C)
    target_compile_definitions(${name} PRIVATE __PUREDARWIN__)
    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
endfunction()
