add_library(ld_libpasses STATIC)
target_sources(ld_libpasses PRIVATE
    branch_island.cpp
    branch_shim.cpp
    compact_unwind.cpp
    dtrace_dof.cpp
    dylibs.cpp
    got.cpp
    huge.cpp
    objc.cpp
    order.cpp
    tlvp.cpp
    stubs/stubs.cpp
    bitcode_bundle.cpp
    code_dedup.cpp
    thread_starts.cpp
    inits.cpp
)

set_property(TARGET ld_libpasses PROPERTY OUTPUT_NAME ld_passes)
target_compile_definitions(ld_libpasses PRIVATE __DARWIN_UNIX03)
target_include_directories(ld_libpasses PRIVATE
    ../../../../include
    ../../../../include/foreign
    ../../abstraction
    ..
    ../..
    ../parsers
    ../../3rd
    ../../..
)
set_property(TARGET ld_libpasses PROPERTY CXX_STANDARD 11)
