add_darwin_static_library(mDNSResponder_dnssd_static)

target_sources(mDNSResponder_dnssd_static PRIVATE
    mDNSShared/dnssd_clientstub.c   # the DNSService* client API
    mDNSShared/dnssd_clientlib.c    # TXTRecord*/name-construction helpers
    mDNSShared/dnssd_ipc.c          # request/reply marshalling
)

target_include_directories(mDNSResponder_dnssd_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mDNSShared
)

target_compile_definitions(mDNSResponder_dnssd_static PRIVATE
    __PUREDARWIN__
    # cdefs.h's generated libc-features.h #errors out unless this matches what
    # libc_static itself was compiled with (see libc/CMakeLists.txt).
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
)

# Same bridgeos-availability shim libc_static force-includes: private headers
# pulled in transitively reference __API_AVAILABLE(...bridgeos(x)...), which
# neither the SDK nor PD define.
target_compile_options(mDNSResponder_dnssd_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>)

target_include_directories(mDNSResponder_dnssd_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_link_libraries(mDNSResponder_dnssd_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)

add_dependencies(mDNSResponder_dnssd_static libc_features)