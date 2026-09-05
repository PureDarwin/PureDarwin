add_darwin_executable(ping MACOSX_VERSION_MIN 26.5)

get_target_property(_ping_opts ping COMPILE_OPTIONS)
if(_ping_opts)
    list(REMOVE_ITEM _ping_opts -nostdinc)
    set_target_properties(ping PROPERTIES COMPILE_OPTIONS "${_ping_opts}")
endif()

target_sources(ping PRIVATE
    ping.c
    # -D timestamp printing calls gmt2local(); network_cmds keeps the
    # implementation in ecnprobe/ rather than ping.tproj/.
    gmt2local.c
)

target_compile_definitions(ping PRIVATE PRIVATE=1 __DARWIN_ONLY_UNIX_CONFORMANCE=1)
# arm64 has no legacy ABI suffixes: libc is built with these (see libc's
# libc_public_headers), so its symbols are the plain names. Without matching
# them here, <sys/cdefs.h> leaves __DARWIN_ONLY_VERS_1050 at 0 and the header
# aliases select() to _select$1050, which exists only on x86.
if(PUREDARWIN_ARCH STREQUAL "arm64")
    target_compile_definitions(ping PRIVATE
        __DARWIN_ONLY_VERS_1050=1
        __DARWIN_ONLY_64_BIT_INO_T=1)
endif()

target_compile_options(ping PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    # xnu's bsd tree for <netinet/ip_var.h>, <netinet/in_systm.h> and
    # <netinet/ip_icmp.h>; libresolv's include for <ifaddrs.h> (the real
    # getifaddrs() is already in libinfo, inside libSystem).
    -I${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
    -I${CMAKE_SOURCE_DIR}/src/Libraries/libresolv/include
    -fno-builtin
    -fno-stack-protector
)

target_link_options(ping PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(ping libSystem_B_stub dyld)

install(TARGETS ping DESTINATION bin COMPONENT BaseSystem)
