# netstat(1) from Apple's network_cmds-741.100.2, unchanged
add_darwin_executable(netstat MACOSX_VERSION_MIN 26.5)

get_target_property(_netstat_opts netstat COMPILE_OPTIONS)
if(_netstat_opts)
    list(REMOVE_ITEM _netstat_opts -nostdinc)
    set_target_properties(netstat PROPERTIES COMPILE_OPTIONS "${_netstat_opts}")
endif()

target_sources(netstat PRIVATE
    bpf.c data.c if.c inet.c inet6.c ipsec.c main.c mbuf.c mcast.c misc.c
    mptcp.c route.c systm.c tp_astring.c unix.c vsock.c
    network_cmds_lib.c
)

target_compile_definitions(netstat PRIVATE
    PRIVATE=1 INET6 IPSEC USE_RFC2292BIS=1 __APPLE_USE_RFC_3542=1
    __APPLE_API_OBSOLETE=1 __DARWIN_ONLY_UNIX_CONFORMANCE=1)
if(PUREDARWIN_ARCH STREQUAL "arm64" OR PUREDARWIN_ARCH STREQUAL "riscv64")
    target_compile_definitions(netstat PRIVATE
        __DARWIN_ONLY_VERS_1050=1
        __DARWIN_ONLY_64_BIT_INO_T=1)
endif()

target_compile_options(netstat PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${CMAKE_CURRENT_SOURCE_DIR}
    -I${CMAKE_CURRENT_SOURCE_DIR}/shim
    # xnu's bsd tree for the private net, netinet and sys headers netstat reads
    -I${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
    -I${CMAKE_SOURCE_DIR}/src/Libraries/libresolv/include
    -fno-builtin
    -fno-stack-protector
    # main.c passes pr_name and pr_protocol to pr_reinit in swapped order, the
    # reinit routines ignore their arguments
    -Wno-error=int-conversion
)

target_link_options(netstat PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(netstat libSystem_B_stub dyld)

install(TARGETS netstat DESTINATION sbin COMPONENT BaseSystem)
