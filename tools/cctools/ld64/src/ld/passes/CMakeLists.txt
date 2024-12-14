add_library(host_ld_libpasses STATIC)
target_sources(host_ld_libpasses PRIVATE
    branch_island.cpp
    branch_shim.cpp
    compact_unwind.cpp
    dtrace_dof.cpp
    dylibs.cpp
    got.cpp
    huge.cpp
    objc.cpp
    objc_constants.cpp
    order.cpp
    tlvp.cpp
    stubs/stubs.cpp
    bitcode_bundle.cpp
    code_dedup.cpp
    thread_starts.cpp
    inits.cpp
)

target_compile_definitions(host_ld_libpasses PRIVATE __DARWIN_UNIX03)
target_include_directories(host_ld_libpasses PRIVATE
    ../../../../include
    ../../../../include/foreign
    ../../abstraction
    ..
    ../..
    ../parsers
    ../../3rd
    ../../..
)
set_property(TARGET host_ld_libpasses PROPERTY CXX_STANDARD 11)
suppress_all_warnings(host_ld_libpasses)
