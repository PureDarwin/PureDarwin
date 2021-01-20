externalproject_add(cctools_host.extproj
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    GIT_REPOSITORY https://github.com/tpoechtrager/cctools-port
    GIT_TAG origin/949.0.1-ld64-530
    CONFIGURE_COMMAND
        <SOURCE_DIR>/cctools/configure --prefix=${PUREDARWIN_TOOLS_DIR}
            --target=x86_64-apple-darwin20 --with-libxar=${PUREDARWIN_TOOLS_DIR}
            --enable-xar-support
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
add_dependencies(cctools_host.extproj xar_host.extproj)

add_executable(ld_host IMPORTED)
add_dependencies(ld_host cctools_host.extproj)
set_property(TARGET ld_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-ld)

add_executable(ar_host IMPORTED)
add_dependencies(ar_host cctools_host.extproj)
set_property(TARGET ar_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-ar)

add_executable(codesign_allocate_host IMPORTED)
add_dependencies(codesign_allocate_host cctools_host.extproj)
set_property(TARGET codesign_allocate_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-codesign_allocate)

add_executable(ctf_insert_host IMPORTED)
add_dependencies(ctf_insert_host cctools_host.extproj)
set_property(TARGET ctf_insert_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-ctf_insert)

add_executable(install_name_tool_host IMPORTED)
add_dependencies(install_name_tool_host cctools_host.extproj)
set_property(TARGET install_name_tool_host PROPERTY IMPORTED_LOCATION ${PUREDARWIN_TOOLS_DIR}/bin/x86_64-apple-darwin20-install_name_tool)
