add_darwin_static_library(XPC_libinfo_static)
target_sources(XPC_libinfo_static PRIVATE
    si_module.c
    search_module.c
    cache_module.c
    file_module.c
    si_data.c
    si_compare.c
    kvbuf.c
    thread_data.c
    libinfo.c
    si_getaddrinfo.c
    getnameinfo_link.c
    if_indextoname.c
    ils.c
    pd_mdns_module_stub.c
)

target_compile_definitions(XPC_libinfo_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
target_compile_options(XPC_libinfo_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include)

target_include_directories(XPC_libinfo_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/stdtime/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/os
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CrashReporterClient/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/pd-compat-include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
)
target_link_libraries(XPC_libinfo_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(XPC_libinfo_static libc_features libsystem_kernel_static)

add_library(XPC_libinfo_headers INTERFACE)
target_include_directories(XPC_libinfo_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
