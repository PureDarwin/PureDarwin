add_darwin_executable(configd MACOSX_VERSION_MIN 26.5)

mig(../SystemConfiguration.fproj/config.defs
    INCLUDES_FROM_TARGET configd
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/configUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/configServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/configServer.c
    ARCH ${PUREDARWIN_ARCH}
    NOVOUCHERS)

target_sources(configd PRIVATE
    configd.m
    configd_server.c
    plugin_support.c
    pattern.c
    session.c
    _SCD.c
    _configadd.c
    _configclose.c
    _configget.c
    _configlist.c
    _confignotify.c
    _configopen.c
    _configremove.c
    _configset.c
    _configunlock.c
    _notifyadd.c
    _notifycancel.c
    _notifychanges.c
    _notifyremove.c
    _notifyviafd.c
    _notifyviaport.c
    _snapshot.c
    ${CMAKE_CURRENT_BINARY_DIR}/configServer.c
    # The server halves of the dnsinfo and nwi XPC protocols. These use
    # CoreFoundation, so unlike their clients (which live in
    # libSystemConfiguration.a inside libSystem) they belong here in configd.
    ../../libSystemConfiguration/dnsinfo/dnsinfo_create.c
    ../../libSystemConfiguration/dnsinfo/dnsinfo_server.c
    ../../libSystemConfiguration/dnsinfo/dnsinfo_flatfile.c
    ../../libSystemConfiguration/nwi/network_information_server.c
    ../../libSystemConfiguration/libSystemConfiguration/libSystemConfiguration_server.c
    ../IPMonitorControl/IPMonitorControlServer.c
    ../Plugins/common/IPMonitorControlPrefs.c
    ../Plugins/common/InterfaceNamerControlPrefs.c
    ../config-agent-info/config_agent_info.c
    ../pd_awd_report_stub.c
    ../Plugins/pd_ipmonitor_absent.c
)

set(_configd_plugins
    PreferencesMonitor
    LinkConfiguration
    KernelEventMonitor
    IPMonitor
    InterfaceNamer
)

# IPMonitor's NAT64 support is the only source in the plugin that genuinely
# needs Apple's libnetwork (the nw_* object API); everything else, including the
# DNS configuration that feeds dnsinfo, is plain C. Its ObjC config agents need
# the network-agent APIs, which are likewise absent.
set(_configd_plugin_excludes
    nat64-configuration.c
    # The ObjC config agents drive libnetwork's network-agent machinery through
    # Foundation; neither is present.
    agent-monitor.m
    configAgent.m
    dnsAgent.m
    proxyAgent.m
    controller.m
    # set-hostname.c resolves the primary address through SCNetworkReachability,
    # which needs libnetwork.
    set-hostname.c
    # smb-configuration.c needs <smb_server_prefs.h> from Apple's smb project.
    smb-configuration.c
)

foreach(_plugin ${_configd_plugins})
    # .m as well as .c: several plugin sources carry the ObjC extension but hold
    # plain C (KernelEventMonitor's ev_extra.m is only ioctl wrappers).
    file(GLOB _plugin_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/../Plugins/${_plugin}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../Plugins/${_plugin}/*.m)
    foreach(_excluded ${_configd_plugin_excludes})
        list(FILTER _plugin_sources EXCLUDE REGEX "/${_excluded}$")
    endforeach()
    target_sources(configd PRIVATE ${_plugin_sources})
    target_compile_definitions(configd PRIVATE HAVE_PLUGIN_${_plugin}=1)
    # Each plist carries CFBundleIdentifier as the literal Xcode build variable
    # $(PRODUCT_BUNDLE_IDENTIFIER), which Xcode substitutes from the target's
    # settings at build time. Installed verbatim it stays unexpanded,
    # CFBundleGetIdentifier() returns NULL, and plugin_support.c skips the
    # bundle with "no bundle ID" - so configd loads no plugins at all. Substitute
    # the identifier the builtin table expects, which is what CFBundleName in
    # these same plists already spells out.
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/../Plugins/${_plugin}/Info.plist _plugin_plist)
    string(REPLACE "$(PRODUCT_BUNDLE_IDENTIFIER)"
        "com.apple.SystemConfiguration.${_plugin}" _plugin_plist "${_plugin_plist}")
    file(GENERATE
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/plugin-plists/${_plugin}/Info.plist
        CONTENT "${_plugin_plist}")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugin-plists/${_plugin}/Info.plist
        DESTINATION System/Library/SystemConfiguration/${_plugin}.bundle/Contents
        COMPONENT BaseSystem)
endforeach()

target_include_directories(configd PRIVATE
    # Security publishes libDER's headers here, under the security_libDER/
    # spelling its own SecCertificatePriv.h uses.
    ${PUREDARWIN_SECURITY_PREFIX}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Plugins/common
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}                                      # config.h, configServer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../pd-compat-include                 # os/state_private.h, ne_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration.fproj         # SCDynamicStoreInternal.h et al
    ${CMAKE_CURRENT_SOURCE_DIR}/../IPMonitorControl                  # IPMonitorControl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../IPMonitorControl/AWD               # AWDIPMonitor*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Plugins/IPMonitor                  # ip_plugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../config-agent-info                  # config_agent_info.h
    # XPC/notify before libresolv/include: the latter carries a cut-down
    # notify.h stub, written before PureDarwin had a notifyd, which would
    # otherwise shadow the real header.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../XPC/notify                      # real notify.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libresolv/include              # ifaddrs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libresolv                       # resolv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mDNSResponder/mDNSShared         # dns_sd_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ppp/include                      # ppp/ppp_msg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystemConfiguration/nwi      # network_information.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystemConfiguration/libSystemConfiguration
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystemConfiguration/dnsinfo  # dnsinfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CommonCrypto/include            # CommonCrypto/CommonDigest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../XPC/libinfo                     # netdb_async.h, si_compare.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../pd-compat-include                 # nw/private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration.fproj/helper  # SCHelper_client.h
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IONetworkingFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    # configd.m includes <objc/objc-runtime.h> but uses nothing from it; the
    # header alone is enough, no libobjc link required.
    ${PUREDARWIN_LIBOBJC_PREFIX}/usr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libsystem_trace/os-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/include
    # xnu's os/log_private.h must win over libc's pd-compat-include stand-in,
    # which redefines the os_log_pack types libsystem_trace's os/log.h supplies.
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${CMAKE_CURRENT_SOURCE_DIR}/../../XPC/libinfo/compat                 # dispatch/private.h
    ${CMAKE_SOURCE_DIR}/src/Libraries/libdarwin/pd-compat-include        # bsm/libbsm.h
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/launchd/System-compat            # sandbox.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CrashReporterClient/include
    # IOKitUser (Apple's CoreFoundation-based IOKitLib) must come before both
    # PureDarwin's CF-free PDIOKitLib headers and xnu's kernel-side IOKit, or
    # InterfaceNamer gets the kernel IOTypes.h and loses IONotificationPortRef,
    # IORegistryEntryIDMatching and IOConnectSetCFProperties.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../IOKit/iokituser/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../IOKit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../XPC/libxpc/include             # vproc.h, vproc_priv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/locale          # xlocale_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/locale/FreeBSD  # setlocale.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/stdtime/FreeBSD # timelocal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/private_include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/iokit
)

target_compile_definitions(configd PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
    HAVE_PLUGIN_IPConfiguration=1
)

target_compile_options(configd PRIVATE
    # <Security/*.h> comes from the built framework's own Headers, which carry
    # Apple's real API; the source tree's include/Security only holds the
    # umbrella and the two placeholder headers.
    -F${PUREDARWIN_SECURITY_PREFIX}/System/Library/Frameworks
    -fblocks
    -F${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../pd_sc_prelude.h>
    -w)

target_link_options(configd PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation"
    $<TARGET_FILE:SystemConfiguration>
    "${PUREDARWIN_SECURITY_PREFIX}/System/Library/Frameworks/Security.framework/Versions/A/Security"
    $<TARGET_FILE:CrashReporterClient>
    # InterfaceNamer talks to IONetworkStack's user client and walks the
    # registry, so it needs IOKitUser's CF-based IOKitLib.
    -Wl,-L,${PUREDARWIN_IOKIT_PREFIX}/usr/lib
    -lIOKitCF
    # IPConfiguration's report_symptoms.c submits DHCP acquisition symptoms.
    "${PUREDARWIN_SYMPTOMREPORTER_PREFIX}/System/Library/PrivateFrameworks/SymptomReporter.framework/Versions/A/SymptomReporter"
    -lSystem)

# IPConfiguration, the DHCP client, lives in the bootp project rather than
# under Plugins/, so it is linked as its own library rather than joining the
# _configd_plugins loop below. Its Info.plist is installed by bootp.
# force_load because the linker only pulls archive members that resolve an
# undefined symbol: server.c is reached solely through server_init(), so a name
# collision was enough to drop it and the MIG server it carries, silently.
target_link_options(configd PRIVATE
    -Wl,-force_load,$<TARGET_FILE:ipconfiguration_static>)

target_link_libraries(configd PRIVATE
    ipconfiguration_static
    bootplib_static
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

add_dependencies(configd SystemConfiguration libSystem_B_stub dyld CrashReporterClient)

install(TARGETS configd DESTINATION usr/libexec COMPONENT BaseSystem)
