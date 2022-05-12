# ld64. This will be our linker if we can make it build properly.
# Currently, I make libprunetrie.a so that we can build the strip tool in cctools.

# BEGIN STATIC LIBRARY libprunetrie

add_library(libprunetrie STATIC)

target_sources(libprunetrie PRIVATE
    src/src/other/PruneTrie.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/configure.h
)

target_include_directories(libprunetrie PUBLIC include_libprunetrie)
target_include_directories(libprunetrie PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(libprunetrie PRIVATE src/src/abstraction)

# TODO: Better source tracking
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    COMMAND DERIVED_FILE_DIR=${CMAKE_CURRENT_BINARY_DIR}
        RC_ProjectSourceVersion=609
        ${CMAKE_CURRENT_SOURCE_DIR}/src/src/create_configure
    COMMENT "Create configure.h"
)

# TODO: DT_TOOLCHAIN_DIR

# END STATIC LIBRARY libprunetrie
