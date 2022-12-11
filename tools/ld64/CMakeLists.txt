# ld64. This will be our linker if we can make it build properly.
# There are a few binaries, and a static library for cctools built from here.

# TODO: Better source tracking
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    COMMAND DERIVED_FILE_DIR=${CMAKE_CURRENT_BINARY_DIR}
        RC_ProjectSourceVersion=609
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/create_configure
    COMMENT "Create configure.h"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/compile_stubs.h
    COMMAND DERIVED_FILE_DIR=${CMAKE_CURRENT_BINARY_DIR}
        SRCROOT=${CMAKE_CURRENT_SOURCE_DIR}/ld64
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/create_compile_stubs
    COMMENT "Create compile_stubs.h"
)

add_library(ld64_headers INTERFACE)

target_sources(ld64_headers PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    ${CMAKE_CURRENT_BINARY_DIR}/compile_stubs.h
)

target_include_directories(ld64_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

macro(add_tool named)
    add_executable(${named})

    target_include_directories(${named} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/abstraction
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld

        ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_compile_options(${named} PRIVATE -Wno-invalid-offsetof)

    install(TARGETS ${named} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)
endmacro()

# BEGIN EXECUTABLE LD

add_tool(ld)

target_sources(ld PRIVATE
    ld64/src/ld/passes/bitcode_bundle.cpp
    ld64/src/ld/passes/code_dedup.cpp
    ld64/src/ld/passes/thread_starts.cpp
    ld64/src/ld/passes/dtrace_dof.cpp
    ld64/src/ld/passes/compact_unwind.cpp
    ld64/src/ld/passes/got.cpp
    ld64/src/ld/passes/huge.cpp
    ld64/src/ld/passes/inits.cpp
    ld64/src/ld/passes/order.cpp
    ld64/src/ld/passes/branch_island.cpp
    ld64/src/ld/passes/objc.cpp
    ld64/src/ld/passes/dylibs.cpp
    ld64/src/ld/passes/objc_constants.cpp
    ld64/src/ld/passes/tlvp.cpp
    ld64/src/ld/passes/branch_shim.cpp

    ld64/src/ld/passes/stubs/stubs.cpp

    ld64/src/ld/parsers/archive_file.cpp
    ld64/src/ld/parsers/lto_file.cpp
    ld64/src/ld/parsers/macho_dylib_file.cpp
    ld64/src/ld/parsers/macho_relocatable_file.cpp
    ld64/src/ld/parsers/textstub_dylib_file.cpp
    ld64/src/ld/parsers/opaque_section_file.cpp
    ld64/src/ld/parsers/generic_dylib_file.cpp

    ld64/src/ld/Options.cpp
    ld64/src/ld/ResponseFiles.cpp
    ld64/src/ld/libcodedirectory.c
    ld64/src/ld/PlatformSupport.cpp
    ld64/src/ld/ld.cpp
    ld64/src/ld/Snapshot.cpp
    ld64/src/ld/debugline.c
    ld64/src/ld/InputFiles.cpp
    ld64/src/ld/SymbolTable.cpp
    ld64/src/ld/Resolver.cpp
    ld64/src/ld/OutputFile.cpp

    ld64/src/ld/code-sign-blobs/blob.cpp

    ld64/src/tapi_stub.cpp
    ld64/src/ld_version.c
)

target_include_directories(ld PRIVATE ${RC_HOST_BIN}/../include)
target_include_directories(ld PRIVATE include include_notapi)

target_include_directories(ld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld/code-sign-blobs
    ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld/passes
    ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld/passes/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld/parsers
    ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld/parsers/libunwind
)

target_compile_options(ld PRIVATE -Wno-undef-prefix -Wno-deprecated-declarations -Wno-return-stack-address)

target_link_options(ld PRIVATE "LINKER:-exported_symbol,__mh_execute_header" "LINKER:-stack_size,0x01000000")
target_link_libraries(ld PRIVATE ld64_headers)
target_link_libraries(ld PRIVATE libxar)

# TODO: Better version tracking
target_compile_definitions(ld PRIVATE LD_VERS=\"ld64-609\")

# END EXECUTABLE LD

# BEGIN EXECUTABLE OBJCIMAGEINFO

add_tool(objcimageinfo)

target_sources(objcimageinfo PRIVATE
    ld64/src/other/objcimageinfo.cpp
)

# BEGIN EXECUTABLE OBJCIMAGEINFO

# BEGIN EXECUTABLE UNWINDDUMP

add_tool(unwinddump)

target_sources(unwinddump PRIVATE
    ld64/src/other/UnwindDump.cpp
)

target_link_options(unwinddump PRIVATE "LINKER:-exported_symbol,__mh_execute_header")

# END EXECUTABLE UNWINDDUMP

# BEGIN EXECUTABLE MACHOCHECK

add_tool(machocheck)

target_sources(machocheck PRIVATE
    ld64/src/other/machochecker.cpp
)

# END EXECUTABLE MACHOCHECK

# BEGIN EXECUTABLE DYLDINFO

add_tool(dyldinfo)

target_sources(dyldinfo PRIVATE
    ld64/src/other/dyldinfo.cpp
)

# END EXECUTABLE DYLDINFO

# BEGIN STATIC LIBRARY libprunetrie

add_library(libprunetrie STATIC)

target_sources(libprunetrie PRIVATE
    ld64/src/other/PruneTrie.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/configure.h
)

target_include_directories(libprunetrie PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}

    ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/abstraction
)

target_include_directories(libprunetrie PUBLIC include_libprunetrie)

# END STATIC LIBRARY libprunetrie
