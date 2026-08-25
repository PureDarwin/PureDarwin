if(NOT PUREDARWIN_ENABLE_SYSTEMCONFIGURATION)
    return()
endif()

set(_bootp_includes
    ${CMAKE_CURRENT_SOURCE_DIR}/bootplib
    ${CMAKE_CURRENT_SOURCE_DIR}/ipconfiglib
    ${CMAKE_CURRENT_SOURCE_DIR}/IPConfiguration.bproj
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration/SystemConfiguration.fproj
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration/IPMonitorControl
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_trace/os-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CrashReporterClient/include
    ${PUREDARWIN_SECURITY_PREFIX}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOKit/iokituser/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/iokit
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libinfo/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/notify
    ${CMAKE_CURRENT_SOURCE_DIR}/../dyld/upstream/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/include/Private
    ${CMAKE_CURRENT_SOURCE_DIR}/../eap8021x/include
    # report_symptoms.c includes <SymptomReporter/SymptomReporter.h>
    ${CMAKE_CURRENT_SOURCE_DIR}/../SymptomReporter/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystemConfiguration/dnsinfo
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IONetworkingFamily/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libdarwin/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libresolv/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD
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

set(_bootp_definitions
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
    TARGET_OS_OSX=1
    # IPV6_PKTINFO/IPV6_HOPLIMIT and the rest of the RFC 3542 ancillary-data
    # API are behind this opt-in in <netinet6/in6.h>; the DHCPv6 and router
    # advertisement sockets need them.
    __APPLE_USE_RFC_3542=1
)

set(_bootp_options
    -w
    -F${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks
    -F${PUREDARWIN_SECURITY_PREFIX}/System/Library/Frameworks
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
)

# genoptionfiles.c is a build-time generator Apple runs to produce the DHCP
# option tables; the generated files are already in the tree.
add_darwin_static_library(bootplib_static)
set_target_properties(bootplib_static PROPERTIES OUTPUT_NAME bootp)

file(GLOB _bootplib_sources ${CMAKE_CURRENT_SOURCE_DIR}/bootplib/*.c)
list(FILTER _bootplib_sources EXCLUDE REGEX "genoptionfiles\\.c$")
target_sources(bootplib_static PRIVATE ${_bootplib_sources})

target_include_directories(bootplib_static PRIVATE ${_bootp_includes})
target_compile_definitions(bootplib_static PRIVATE ${_bootp_definitions})
target_compile_options(bootplib_static PRIVATE ${_bootp_options})
target_link_libraries(bootplib_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)
add_dependencies(bootplib_static libc_features)

mig(bootplib/ipconfig.defs
    INCLUDES_FROM_TARGET bootplib_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/ipconfig.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/ipconfigUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/ipconfigServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/ipconfigServer.c
    ARCH ${PUREDARWIN_ARCH}
    NOVOUCHERS)

add_darwin_static_library(ipconfiguration_static)
set_target_properties(ipconfiguration_static PROPERTIES OUTPUT_NAME ipconfiguration)

file(GLOB _ipconfig_sources ${CMAKE_CURRENT_SOURCE_DIR}/IPConfiguration.bproj/*.c)
target_sources(ipconfiguration_static PRIVATE
    ${_ipconfig_sources}
    ${CMAKE_CURRENT_BINARY_DIR}/ipconfigServer.c)

target_include_directories(ipconfiguration_static PRIVATE ${_bootp_includes})
target_compile_definitions(ipconfiguration_static PRIVATE
    PUREDARWIN_CONFIGD=1
    ${_bootp_definitions}
    server_init=IPConfiguration_server_init)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/IPConfiguration.bproj/ipconfigd.c
    PROPERTIES COMPILE_DEFINITIONS
    "load=load_IPConfiguration;start=start_IPConfiguration;prime=prime_IPConfiguration;stop=stop_IPConfiguration")
target_compile_options(ipconfiguration_static PRIVATE ${_bootp_options})
target_link_libraries(ipconfiguration_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)
add_dependencies(ipconfiguration_static libc_features)

# ipconfig: the command line client. SystemStarter runs `ipconfig waitall` to
# block until every interface has finished configuring.
add_darwin_executable(ipconfig MACOSX_VERSION_MIN 11.0)

target_sources(ipconfig PRIVATE
    ipconfig.tproj/client.c
    ipconfiglib/IPConfigurationUtil.c
    ${CMAKE_CURRENT_BINARY_DIR}/ipconfigUser.c)

target_include_directories(ipconfig PRIVATE ${_bootp_includes})
target_compile_definitions(ipconfig PRIVATE ${_bootp_definitions})
target_compile_options(ipconfig PRIVATE ${_bootp_options})

target_link_options(ipconfig PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-platform_version,macos,11.0,11.5
    -Wl,-dylinker_install_name,/usr/lib/dyld
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation"
    -Wl,-L,${PUREDARWIN_IOKIT_PREFIX}/usr/lib
    -lIOKitCF
    $<TARGET_FILE:SystemConfiguration>
    -lSystem)

target_link_libraries(ipconfig PRIVATE
    bootplib_static
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

add_dependencies(ipconfig libc_features libSystem_B_stub dyld SystemConfiguration)

install(TARGETS ipconfig DESTINATION usr/sbin COMPONENT BaseSystem)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/IPConfiguration.bproj/Info.plist _ipconfig_plist)
string(REPLACE "$(PRODUCT_BUNDLE_IDENTIFIER)"
    "com.apple.SystemConfiguration.IPConfiguration" _ipconfig_plist "${_ipconfig_plist}")
string(REPLACE "<dict>\n\t<key>CFBundleDevelopmentRegion</key>"
    "<dict>\n\t<key>Builtin</key>\n\t<true/>\n\t<key>CFBundleDevelopmentRegion</key>"
    _ipconfig_plist "${_ipconfig_plist}")
file(GENERATE
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/plugin-plists/IPConfiguration/Info.plist
    CONTENT "${_ipconfig_plist}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugin-plists/IPConfiguration/Info.plist
    DESTINATION System/Library/SystemConfiguration/IPConfiguration.bundle/Contents
    COMPONENT BaseSystem)
