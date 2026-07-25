mig(job.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/job.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/job_user.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/jobServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/jobServer.c
    NOVOUCHERS)
mig(job_forward.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/job_forward.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/job_forward_user.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/job_forwardServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/job_forwardServer.c
    NOVOUCHERS)
mig(job_reply.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/job_reply.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/job_replyUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/job_reply_server.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/job_replyServer.c
    NOVOUCHERS)
mig(notify.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/notifyServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/notifyServer.c
    NOVOUCHERS)
mig(mach_exc.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
    NOVOUCHERS)
mig(internal.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internal.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internal_user.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/internalServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/internalServer.c
    NOVOUCHERS)
mig(domain.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/domain.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/domain_user.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/domainServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/domainServer.c
    NOVOUCHERS)

add_library(XPC_launchd_mig_static STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/job_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/jobServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/job_forward_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/job_forwardServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/job_replyUser.c
    ${CMAKE_CURRENT_BINARY_DIR}/job_replyServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/internal_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/internalServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/domain_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/domainServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/notifyServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
)
target_compile_definitions(XPC_launchd_mig_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)
target_compile_options(XPC_launchd_mig_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/core.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include)
target_include_directories(XPC_launchd_mig_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
)
add_dependencies(XPC_launchd_mig_static libsystem_kernel_static)
target_link_libraries(XPC_launchd_mig_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(XPC_launchd_mig_static libc_features)

add_library(XPC_launchd_mig_headers INTERFACE)
target_include_directories(XPC_launchd_mig_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

add_darwin_static_library(XPC_launchd_static)
target_sources(XPC_launchd_static PRIVATE
    runtime.c
    log.c
    kill2.c
    ktrace.c
    ipc.c
    core.c
    launchd.c
    pd_sandbox_check.c
    pd_job_forward_server.c
    pd_launchd_boot.c
    pd_launchd_plist.c
)
target_compile_definitions(XPC_launchd_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)
target_compile_options(XPC_launchd_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include
    # pd_launchd_plist.c's <CoreFoundation/CFData.h>-style includes need
    # real framework-header resolution (CF's own headers are written the
    # same self-referential way, e.g. CFDictionary.h includes
    # <CoreFoundation/CFBase.h>). -nostdinc (added by
    # add_darwin_static_library() above) only suppresses clang's *implicit*
    # default search paths, not explicitly-added ones, so this -F is
    # unaffected by it and doesn't touch core.c/launchd.c's carefully
    # isolated host-include setup.
    -F${PUREDARWIN_MACOSX_SDK}/System/Library/Frameworks
    -ferror-limit=0)
target_include_directories(XPC_launchd_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/System-compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../libinfo
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc/include
    ${CMAKE_BINARY_DIR}/src/Libraries/XPC/libxpc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libnv
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CrashReporterClient/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
)
target_link_libraries(XPC_launchd_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(XPC_launchd_static libc_features libsystem_kernel_static XPC_launchd_mig_static XPC_libxpc_static XPC_libinfo_static)
