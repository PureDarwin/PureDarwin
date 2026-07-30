add_darwin_static_library(libSystemConfiguration_static)

target_sources(libSystemConfiguration_static PRIVATE
    dnsinfo/dnsinfo_copy.c
    nwi/network_information.c
    nwi/network_state_information_priv.c
    libSystemConfiguration/libSystemConfiguration_client.c
)

target_include_directories(libSystemConfiguration_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/dnsinfo
    ${CMAKE_CURRENT_SOURCE_DIR}/nwi
)

target_include_directories(libSystemConfiguration_static PRIVATE
    # SCPrivate.h stand-in; must precede any real SystemConfiguration headers.
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/libSystemConfiguration
    ${CMAKE_CURRENT_SOURCE_DIR}/nwi
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_trace/os-include        # os/log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include                # xpc, vproc_priv
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/notify                        # notify.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonCrypto/include              # CommonCrypto/CommonDigest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../SystemConfiguration/config-agent-info  # config_agent_info.h
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd                           # net/necp.h (struct netagent)
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libinfo/compat                # dispatch/private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_compile_definitions(libSystemConfiguration_static PRIVATE
    __PUREDARWIN__
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
)

target_compile_options(libSystemConfiguration_static PRIVATE
    -fblocks
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
    -w)

target_link_libraries(libSystemConfiguration_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)

add_dependencies(libSystemConfiguration_static libc_features)
