# mdnsd - the mDNSResponder daemon.
#
# Sources are vendored under src/Libraries/mDNSResponder (alongside the
# libdns_sd client that goes into libSystem); only the daemon binary is built
# here, because userland builds configure with enableLibraries=false.
set(MDNS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../Libraries/mDNSResponder)

add_darwin_executable(mdnsd MACOSX_VERSION_MIN 11.0)

# Apple's sources include the standard headers directly, so drop -nostdinc and
# use the SDK sysroot, the same way the other userland executables do.
get_target_property(_mdnsd_opts mdnsd COMPILE_OPTIONS)
if(_mdnsd_opts)
    list(REMOVE_ITEM _mdnsd_opts -nostdinc)
    set_target_properties(mdnsd PROPERTIES COMPILE_OPTIONS "${_mdnsd_opts}")
endif()

target_sources(mdnsd PRIVATE
    ${MDNS_SRC}/mDNSPosix/PosixDaemon.c
    ${MDNS_SRC}/mDNSPosix/mDNSPosix.c
    ${MDNS_SRC}/mDNSPosix/mDNSUNP.c
    ${MDNS_SRC}/mDNSPosix/posix_utilities.c
    ${MDNS_SRC}/mDNSCore/mDNS.c
    ${MDNS_SRC}/mDNSCore/DNSDigest.c
    ${MDNS_SRC}/mDNSCore/uDNS.c
    ${MDNS_SRC}/mDNSCore/DNSCommon.c
    ${MDNS_SRC}/mDNSShared/uds_daemon.c
    ${MDNS_SRC}/mDNSShared/mDNSDebug.c
    ${MDNS_SRC}/mDNSShared/dnssd_ipc.c
    ${MDNS_SRC}/mDNSShared/GenLinkedList.c
    ${MDNS_SRC}/mDNSShared/PlatformCommon.c
    ${MDNS_SRC}/mDNSShared/ClientRequests.c
    ${MDNS_SRC}/mDNSShared/dnssd_clientshim.c
    ${MDNS_SRC}/DSO/dso.c
    ${MDNS_SRC}/DSO/dso-transport.c
)

target_include_directories(mdnsd PRIVATE
    ${MDNS_SRC}/mDNSCore
    ${MDNS_SRC}/mDNSShared
    ${MDNS_SRC}/mDNSPosix
    ${MDNS_SRC}/DSO
)

# POSIX_BUILD selects the mDNSPosix platform layer.
#
# NOT_HAVE_SA_LEN must NOT be set: Darwin's sockaddr/sockaddr_un really do carry
# sa_len/sun_len (bsd/sys/socket.h, bsd/sys/un.h) and the kernel reads them.
# Defining it leaves sun_len zero in udsserver_init()'s bind(), which fails, so
# mdnsd skips its main loop entirely and exits immediately after logging
# "starting" - which under KeepAlive turns into a respawn loop.
#
# -w because Apple's sources warn heavily under this toolchain and are not
# worth patching for it.
target_compile_definitions(mdnsd PRIVATE POSIX_BUILD)
target_compile_options(mdnsd PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
    -w
)

target_link_options(mdnsd PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem
)

add_dependencies(mdnsd libSystem_B_stub dyld)

install(TARGETS mdnsd DESTINATION usr/sbin COMPONENT BaseSystem)

install(FILES com.apple.mDNSResponder.plist
        DESTINATION System/Library/LaunchDaemons
        COMPONENT BaseSystem OPTIONAL)
