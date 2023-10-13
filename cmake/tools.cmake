set(tool_targets
    ctfconvert ctfdump ctfmerge xar otool ld64 unifdef migcom
    checksyms lipo size strings nm libtool redo_prebinding
    seg_addr_table seg_hack install_name_tool indr strip segedit
    pagestuff codesign_allocate bitcode_strip ctf_insert check_dylib
    cmpdylib inout vtool nmedit ld
)

foreach(target ${tool_targets})
    list(APPEND byproducts ${CMAKE_BINARY_DIR}/tools/bin/${target})
endforeach()

ExternalProject_Add(tools-extproj
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tools
    PREFIX ${CMAKE_BINARY_DIR}/tools
    BINARY_DIR ${CMAKE_BINARY_DIR}/tools
    CMAKE_ARGS
        -UCMAKE_TOOLCHAIN_FILE
	    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/tools/bin
        -DTOOLS_FOLDER=${CMAKE_BINARY_DIR}/tools/bin
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${byproducts}
    USES_TERMINAL_CONFIGURE TRUE USES_TERMINAL_BUILD TRUE
)

ExternalProject_Get_Property(tools-extproj INSTALL_DIR)

macro(add_imported_tool name)
    add_executable(host_${name} IMPORTED)
    set_property(TARGET host_${name} PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/bin/${name})
    add_dependencies(host_${name} tools-extproj)
endmacro()

foreach(tool ${tool_targets})
    add_imported_tool(${tool})
endforeach()

add_executable(mig IMPORTED)
set_property(TARGET mig PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/tools/mig/mig.sh)
add_dependencies(mig host_migcom)
