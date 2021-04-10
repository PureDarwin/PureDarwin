function(add_darwin_static_library name)
    add_library(${name} STATIC)
    add_dependencies(${name} darwin_libtool)

    string(SUBSTRING ${name} 0 3 name_prefix)
    if(name_prefix STREQUAL "lib")
        set_property(TARGET ${name} PROPERTY PREFIX "")
    endif()

    target_compile_options(${name} PRIVATE -target x86_64-apple-darwin20)
    target_compile_options(${name} PRIVATE -nostdlib -nostdinc)
endfunction()
