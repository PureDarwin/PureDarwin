add_library(host_ld_libparsers STATIC)
target_sources(host_ld_libparsers PRIVATE
    archive_file.cpp
    generic_dylib_file.cpp
    lto_file.cpp
    macho_dylib_file.cpp
    macho_relocatable_file.cpp
    opaque_section_file.cpp
    textstub_dylib_file.cpp
)

target_compile_definitions(host_ld_libparsers PRIVATE __DARWIN_UNIX03)
target_include_directories(host_ld_libparsers PRIVATE
    ../../../../include
    ../../../../include/foreign
    ../../abstraction
    ../../3rd
    ../../..
    ../..
    ..
)
target_link_libraries(host_ld_libparsers PRIVATE host_ld_helper)
set_property(TARGET host_ld_libparsers PROPERTY CXX_STANDARD 11)
suppress_all_warnings(host_ld_libparsers)
