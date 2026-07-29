add_darwin_executable(ping MACOSX_VERSION_MIN 11.0)

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
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem
)

add_dependencies(ping libSystem_B_stub dyld)

install(TARGETS ping DESTINATION bin COMPONENT BaseSystem)
