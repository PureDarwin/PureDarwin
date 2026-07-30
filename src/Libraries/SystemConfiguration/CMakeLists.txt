if(NOT PUREDARWIN_ENABLE_SYSTEMCONFIGURATION)
    return()
endif()

if(NOT PUREDARWIN_COREFOUNDATION_PREFIX AND DEFINED ENV{PUREDARWIN_COREFOUNDATION_PREFIX})
    set(PUREDARWIN_COREFOUNDATION_PREFIX "$ENV{PUREDARWIN_COREFOUNDATION_PREFIX}")
endif()
if(NOT PUREDARWIN_COREFOUNDATION_PREFIX)
    message(FATAL_ERROR "PUREDARWIN_COREFOUNDATION_PREFIX must point to the CoreFoundation install prefix (with include/CoreFoundation and System/Library/Frameworks)")
endif()

add_darwin_shared_library(SystemConfiguration
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration)

mig(SystemConfiguration.fproj/config.defs
    INCLUDES_FROM_TARGET SystemConfiguration
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/config.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/configUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/configServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/configServer.c
    ARCH ${PUREDARWIN_ARCH}
    NOVOUCHERS)

# SCHelper_client.c talks to the SCHelper privileged-operation daemon.
mig(SystemConfiguration.fproj/helper/helper.defs
    INCLUDES_FROM_TARGET SystemConfiguration
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/helper.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/helperUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/helperServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/helperServer.c
    ARCH ${PUREDARWIN_ARCH}
    NOVOUCHERS)

target_sources(SystemConfiguration PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/helperUser.c
    SystemConfiguration.fproj/SCD.c
    SystemConfiguration.fproj/SCDAdd.c
    SystemConfiguration.fproj/SCDCache.c
    SystemConfiguration.fproj/SCDConsoleUser.c
    SystemConfiguration.fproj/SCDGet.c
    SystemConfiguration.fproj/SCDKeys.c
    SystemConfiguration.fproj/SCDList.c
    SystemConfiguration.fproj/SCDNotify.c
    SystemConfiguration.fproj/SCDNotifierAdd.c
    SystemConfiguration.fproj/SCDNotifierCancel.c
    SystemConfiguration.fproj/SCDNotifierGetChanges.c
    SystemConfiguration.fproj/SCDNotifierInformViaCallback.c
    SystemConfiguration.fproj/SCDNotifierInformViaFD.c
    SystemConfiguration.fproj/SCDNotifierRemove.c
    SystemConfiguration.fproj/SCDNotifierSetKeys.c
    SystemConfiguration.fproj/SCDNotifierWait.c
    SystemConfiguration.fproj/SCDOpen.c
    SystemConfiguration.fproj/SCDPlugin.c
    SystemConfiguration.fproj/dy_framework.c
    SystemConfiguration.fproj/scprefs_observer.c
    SystemConfiguration.fproj/helper/SCHelper_client.c
    SystemConfiguration.fproj/SCPreferencesKeychainPrivate.c
    IPMonitorControl/IPMonitorControl.c
    SystemConfiguration.fproj/BondConfiguration.c
    SystemConfiguration.fproj/BridgeConfiguration.c
    SystemConfiguration.fproj/CaptiveNetwork.c
    SystemConfiguration.fproj/DHCP.c
    SystemConfiguration.fproj/LinkConfiguration.c
    SystemConfiguration.fproj/SCDHostName.c
    SystemConfiguration.fproj/SCLocation.c
    SystemConfiguration.fproj/SCNetworkConfigurationInternal.c
    SystemConfiguration.fproj/SCNetworkConfigurationPrivate.c
    SystemConfiguration.fproj/SCNetworkInterface.c
    SystemConfiguration.fproj/SCNetworkInterfaceProvider.c
    SystemConfiguration.fproj/SCNetworkProtocol.c
    SystemConfiguration.fproj/SCNetworkService.c
    SystemConfiguration.fproj/SCNetworkSet.c
    SystemConfiguration.fproj/SCP.c
    SystemConfiguration.fproj/SCPAdd.c
    SystemConfiguration.fproj/SCPApply.c
    SystemConfiguration.fproj/SCPCommit.c
    SystemConfiguration.fproj/SCPGet.c
    SystemConfiguration.fproj/SCPList.c
    SystemConfiguration.fproj/SCPLock.c
    SystemConfiguration.fproj/SCPOpen.c
    SystemConfiguration.fproj/SCPPath.c
    SystemConfiguration.fproj/SCPRemove.c
    SystemConfiguration.fproj/SCPSet.c
    SystemConfiguration.fproj/SCPUnlock.c
    SystemConfiguration.fproj/SCPreferencesPathKey.c
    SystemConfiguration.fproj/VLANConfiguration.c
    SystemConfiguration.fproj/SCDPrivate.c
    SystemConfiguration.fproj/SCDRemove.c
    SystemConfiguration.fproj/SCDSet.c
    SystemConfiguration.fproj/SCDSnapshot.c
    SystemConfiguration.fproj/SCProxies.c
    SystemConfiguration.fproj/SCSchemaDefinitions.c
    ${CMAKE_CURRENT_BINARY_DIR}/configUser.c
    pd_sc_cfurl_resource.c
    pd_fsevents_stub.c
    pd_sc_reachability_stub.c
)

target_include_directories(SystemConfiguration PUBLIC include)

target_include_directories(SystemConfiguration PRIVATE
    # os/state_private.h stand-in; must precede libc's pd-compat-include.
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/SystemConfiguration.fproj
    ${CMAKE_CURRENT_BINARY_DIR}                                  # config.h (MiG)
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_trace/os-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CrashReporterClient/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Security/include
    # Apple's CoreFoundation-based IOKitLib (IOKitUser), not PureDarwin's
    # CF-free PDIOKitLib.
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOKit/iokituser/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/iokit                     # IOKit/IOBSD.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libinfo/compat            # dispatch/private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../dyld/upstream/include         # dlfcn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IPMonitorControl                 # IPMonitorControl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SystemConfiguration.fproj/helper  # SCHelper_client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/include           # CommonCrypto/CommonDigest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../eap8021x/include               # EAP8021X/EAPClientProperties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ppp/include                    # ppp/ppp_msg.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libinfo                    # si_compare.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystemConfiguration/dnsinfo # dnsinfo.h
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IONetworkingFamily/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOStorageFamily/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOSerialFamily/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOUSBFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Plugins/common                    # plugin_shared.h
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/launchd/System-compat     # sandbox.h
    ${CMAKE_SOURCE_DIR}/src/Libraries/libdarwin/pd-compat-include    # bsm/libbsm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/notify                     # real notify.h
    # after XPC/notify: libresolv/include carries a cut-down notify.h stub that
    # would otherwise shadow it. Only ifaddrs.h is wanted from here.
    ${CMAKE_CURRENT_SOURCE_DIR}/../libresolv/include              # ifaddrs.h
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale          # xlocale_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD  # setlocale.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD # timelocal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

# PRIVATE=1 matches libsystem_kernel and libdispatch: <machine/cpu_capabilities.h>,
# reached through os/log_private.h, is entirely inside #ifdef PRIVATE.
target_compile_definitions(SystemConfiguration PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
)

target_compile_options(SystemConfiguration PRIVATE
    -fblocks
    -F${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/pd_sc_prelude.h>
    -w)

target_link_options(SystemConfiguration PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-platform_version,macos,11.0,11.5
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation"
    # gCRAnnotations (__DATA,__crash_info) is defined per image on Darwin, so
    # every binary using CRSetCrashLogMessage links its own copy. By archive
    # path rather than target name: the target's INTERFACE deps include
    # xnu_kernel_headers, which src/Kernel/xnu does not define in this build.
    $<TARGET_FILE:CrashReporterClient>
    "${PUREDARWIN_SECURITY_PREFIX}/System/Library/Frameworks/Security.framework/Versions/A/Security"
    -lSystem)

# xnu_headers/xnu_private_headers are deliberately absent here: they are defined
# by src/Kernel/xnu, which this derivation does not configure, and an unknown
# name in target_link_libraries becomes a literal -lxnu_headers at link time.
# Nothing here needs them - the xnu headers these sources use come in through
# libc_public_headers and the explicit include paths above.
target_link_libraries(SystemConfiguration PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

# libsystem_kernel_static generates the mach/*_server.h headers osfmk's
# mach_interface.h includes.
add_dependencies(SystemConfiguration libc_features libSystem_B_stub dyld libsystem_kernel_static CrashReporterClient)

install(TARGETS SystemConfiguration
    DESTINATION System/Library/Frameworks/SystemConfiguration.framework/Versions/A
    COMPONENT BaseSystem)

add_subdirectory(configd.tproj)
