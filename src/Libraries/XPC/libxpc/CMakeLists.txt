add_darwin_static_library(XPC_libxpc_static)
# gCRAnnotations (CrashReporterClient.c's custom "__DATA,__crash_info"
# section global, used by xpc_debug.c's xpc_api_misuse via
# CRSetCrashLogMessage) never showed up in libSystem.B.dylib's dynamic
# exported-symbol table even with a matching -exported_symbols_list entry.
# Force-loading the real object directly wherever libxpc itself finally
# gets linked (via this INTERFACE link option, so it follows libxpc to any
# future executable, not just launchd) sidesteps that.
add_dependencies(XPC_libxpc_static CrashReporterClient)
target_link_options(XPC_libxpc_static INTERFACE -Wl,-force_load,$<TARGET_FILE:CrashReporterClient>)

mig(helper.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/helper.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/helper_user.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/helperServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/helperServer.c
    NOVOUCHERS)

target_sources(XPC_libxpc_static PRIVATE
    xpc_array.c
    xpc_connection.c
    xpc_debug.c
    xpc_dictionary.c
    xpc_error.c
    xpc_misc.c
    xpc_private.c
    xpc_type.c
    pd_xpc_object.c
    libbootstrap.c
    liblaunch.c
    libvproc.c
    ${CMAKE_CURRENT_BINARY_DIR}/helper_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/helperServer.c
)

target_compile_definitions(XPC_libxpc_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)
target_compile_options(XPC_libxpc_static PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include)
target_compile_options(XPC_libxpc_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>)

target_include_directories(XPC_libxpc_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../launchd
    ${CMAKE_CURRENT_SOURCE_DIR}/../libnv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/stdtime/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CrashReporterClient/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/os
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
)
add_dependencies(XPC_libxpc_static libsystem_kernel_static)

target_link_libraries(XPC_libxpc_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(XPC_libxpc_static libc_features)
# Wired up in ../CMakeLists.txt after launchd/ (which generates job.h etc, and
# depends on this target existing first for ITS OWN mig() calls) is processed.
