set(_otool_selfhost_sources
    ../otool/main.c
    ../otool/ofile_print.c
    ../otool/m68k_disasm.c
    ../otool/m88k_disasm.c
    ../otool/i386_disasm.c
    ../otool/i860_disasm.c
    ../otool/ppc_disasm.c
    ../otool/hppa_disasm.c
    ../otool/sparc_disasm.c
    ../otool/arm_disasm.c
    ../otool/arm64_disasm.c
    ../otool/print_bitcode.c
    ../otool/coff_print.c
    ../otool/dyld_bind_info.c
    ../otool/host_stubs.c
    pd_otool_compat.c
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/stdlib/FreeBSD/reallocf.c
)

add_darwin_executable(otool_selfhost MACOSX_VERSION_MIN 11.0)
get_target_property(_otool_selfhost_opts otool_selfhost COMPILE_OPTIONS)
if(_otool_selfhost_opts)
    list(REMOVE_ITEM _otool_selfhost_opts -nostdinc)
    set_target_properties(otool_selfhost PROPERTIES COMPILE_OPTIONS "${_otool_selfhost_opts}")
endif()
target_sources(otool_selfhost PRIVATE ${_otool_selfhost_sources})
target_include_directories(otool_selfhost PRIVATE
    ../otool
    ../include
    ../include/foreign
    ../libstuff
)
target_compile_definitions(otool_selfhost PRIVATE
    -D__LITTLE_ENDIAN__
    -D__DARWIN_UNIX03=0
    -DPROGRAM_PREFIX=""
)
target_compile_options(otool_selfhost PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    "SHELL:-include stdlib.h"
    "SHELL:-include mach/i386/_structs.h"
)
target_link_options(otool_selfhost PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem
)
target_link_libraries(otool_selfhost PRIVATE libmacho_static libstuff_static)
add_dependencies(otool_selfhost libSystem_B_stub dyld)
set_property(TARGET otool_selfhost PROPERTY OUTPUT_NAME otool)
install(TARGETS otool_selfhost DESTINATION usr/bin COMPONENT BaseSystem)

suppress_all_warnings(otool_selfhost)
