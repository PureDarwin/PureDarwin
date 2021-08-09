find_program(lld_path ld.lld REQUIRED)

function(add_fbsd_executable name)
    add_executable(${name})

    add_dependencies(${name} fbsd_base)
    target_compile_options(${name} PRIVATE -target x86_64-pc-freebsd13.0)
    target_compile_options(${name} PRIVATE -nostdinc -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)
    target_link_options(${name} PRIVATE -target x86_64-pc-freebsd13.0 -fuse-ld=${lld_path})
    target_link_options(${name} PRIVATE -nostdlib -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)
endfunction()

function(add_fbsd_static_library name)
    add_library(${name} STATIC)

    add_dependencies(${name} fbsd_base)
    target_compile_options(${name} PRIVATE -target x86_64-pc-freebsd13.0)
    target_compile_options(${name} PRIVATE -nostdinc -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)
    target_link_options(${name} PRIVATE -target x86_64-pc-freebsd13.0 -fuse-ld=${lld_path})
    target_link_options(${name} PRIVATE -nostdlib -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()
endfunction()

function(add_fbsd_shared_library name)
    cmake_parse_arguments(SL "MODULE" "" "" ${ARGN})

    if(SL_MODULE)
        add_library(${name} MODULE)
    else()
        add_library(${name} SHARED)
    endif()

    add_dependencies(${name} fbsd_base)
    target_compile_options(${name} PRIVATE -target x86_64-pc-freebsd13.0)
    target_compile_options(${name} PRIVATE -nostdinc -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)
    target_link_options(${name} PRIVATE -target x86_64-pc-freebsd13.0 -fuse-ld=${lld_path})
    target_link_options(${name} PRIVATE -nostdlib -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()
endfunction()

function(add_fbsd_object_library name)
    add_library(${name} OBJECT)
    set_property(TARGET ${name} PROPERTY LINKER_LANGUAGE C)

    add_dependencies(${name} fbsd_base)
    target_compile_options(${name} PRIVATE -target x86_64-pc-freebsd13.0)
    target_compile_options(${name} PRIVATE -nostdinc -isysroot ${PUREDARWIN_SOURCE_DIR}/tools/fbsd_base/base)
endfunction()
