add_subdirectory(libmacho)
add_subdirectory(libstuff)
add_subdirectory(misc)
# add_subdirectory(otool) # disabled - doesn't build
add_subdirectory(ld64/src)

add_executable(darwin_ld IMPORTED GLOBAL)
add_dependencies(darwin_ld cctools_host.extproj)
set_property(TARGET darwin_ld PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-ld)

add_executable(darwin_libtool IMPORTED GLOBAL)
add_dependencies(darwin_libtool cctools_host.extproj)
set_property(TARGET darwin_libtool PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-libtool)

add_executable(darwin_codesign_allocate IMPORTED GLOBAL)
add_dependencies(darwin_codesign_allocate cctools_host.extproj)
set_property(TARGET darwin_codesign_allocate PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-codesign_allocate)

add_executable(darwin_ctf_insert IMPORTED GLOBAL)
add_dependencies(darwin_ctf_insert cctools_host.extproj)
set_property(TARGET darwin_ctf_insert PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-ctf_insert)

add_executable(darwin_install_name_tool IMPORTED GLOBAL)
add_dependencies(darwin_install_name_tool cctools_host.extproj)
set_property(TARGET darwin_install_name_tool PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-install_name_tool)
