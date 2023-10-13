add_library(ld_libparsers STATIC)
target_sources(ld_libparsers PRIVATE
    archive_file.cpp
    lto_file.cpp
    macho_dylib_file.cpp
    macho_relocatable_file.cpp
    opaque_section_file.cpp
    textstub_dylib_file.cpp
)

set_property(TARGET ld_libparsers PROPERTY OUTPUT_NAME ld_parsers)
target_compile_definitions(ld_libparsers PRIVATE __DARWIN_UNIX03)
target_include_directories(ld_libparsers PRIVATE
    ../../../../include
    ../../../../include/foreign
    ../../abstraction
    ../../3rd
    ../../..
    ../..
    ..
)
target_link_libraries(ld_libparsers PRIVATE ld_helper)
set_property(TARGET ld_libparsers PROPERTY CXX_STANDARD 11)
