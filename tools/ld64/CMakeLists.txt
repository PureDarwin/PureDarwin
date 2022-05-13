# ld64. This will be our linker if we can make it build properly.
# There are a few binaries, and a static library for cctools built from here.

# TODO: Better source tracking
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    COMMAND DERIVED_FILE_DIR=${CMAKE_CURRENT_BINARY_DIR}
        RC_ProjectSourceVersion=609
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/create_configure
    COMMENT "Create configure.h"
)

macro(add_tool named)
    add_executable(${named})

    target_include_directories(${named} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/abstraction
        ${CMAKE_CURRENT_SOURCE_DIR}/ld64/src/ld

        ${CMAKE_CURRENT_BINARY_DIR}
    )

    set_property(TARGET ${named} PROPERTY CXX_STANDARD 14)

    target_compile_options(${named} PRIVATE -Wno-invalid-offsetof -flto)
    target_link_options(${named} PRIVATE -flto)

    install(TARGETS ${named} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)
endmacro()

# BEGIN EXECUTABLE LD
## TODO
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

target_compile_options(libprunetrie PRIVATE -flto)
target_link_options(libprunetrie PRIVATE -flto)

# TODO: DT_TOOLCHAIN_DIR

# END STATIC LIBRARY libprunetrie
