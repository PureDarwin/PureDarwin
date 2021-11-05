add_compile_options(-Wno-format)

add_subdirectory(libmacho)
add_subdirectory(libstuff)
add_subdirectory(misc)
# add_subdirectory(otool) # disabled - doesn't build
add_subdirectory(ld64/src)

add_executable(darwin_ld ALIAS host_ld)
add_executable(darwin_libtool ALIAS host_libtool)
add_executable(darwin_codesign_allocate ALIAS host_codesign_allocate)
add_executable(darwin_ctf_insert ALIAS host_ctf_insert)
add_executable(darwin_install_name_tool ALIAS host_install_name_tool)
