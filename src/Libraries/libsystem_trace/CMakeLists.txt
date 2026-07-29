# libsystem_trace: os_log(3) / os_signpost(3).
#
# Vendored from ravynOS (Libraries/Libsystem/libsystem_trace). This is Zoe
# Knox's work, vendored for PureDarwin
# Kindly stolen and used kindly. (go steal my work back :P)

add_darwin_static_library(libsystem_trace_static)

target_sources(libsystem_trace_static PRIVATE
    log.c
    signpost.c
    init.c
)

target_include_directories(libsystem_trace_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(libsystem_trace_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/os-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Kernel/xnu/libkern
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CoreFoundation
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale          # xlocale_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD  # setlocale.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD # timelocal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../dyld/upstream/include          # dlfcn.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime # Block.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private          # pthread/tsd_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include # os/tsd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

# Same feature/availability contract every static library joining libSystem
# needs (see libresolv/CMakeLists.txt).
target_compile_definitions(libsystem_trace_static PRIVATE
    __PUREDARWIN__
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    # <machine/cpu_capabilities.h>, reached via firehose/tracepoint_private.h,
    # is entirely inside #ifdef PRIVATE - without this the _COMM_PAGE_* address
    # it reads is never declared. libsystem_kernel_static and libdispatch_static
    # set the same thing.
    PRIVATE=1
)

target_compile_options(libsystem_trace_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
    -w)

target_link_libraries(libsystem_trace_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)

add_dependencies(libsystem_trace_static libc_features)
