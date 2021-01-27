set(CMAKE_CROSS_CREATE_STATIC_LIBRARY "${PUREDARWIN_BINARY_DIR}/tools/prefix/bin/x86_64-apple-darwin20-libtool -static -o <TARGET> <OBJECTS>")

function(add_darwin_static_library name)
    add_library(${name} STATIC)
    add_dependencies(${name} darwin_libtool)
    set_property(TARGET ${name} PROPERTY LINKER_LANGUAGE CROSS)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()

    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
endfunction()

function(add_darwin_shared_library name)
    add_library(${name} SHARED)
    add_dependencies(${name} darwin_ld)
    target_link_options(${name} PRIVATE -fuse-ld=$<TARGET_FILE:darwin_ld>)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()

    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
endfunction()
