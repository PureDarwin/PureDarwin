mig(aslcommon/asl_ipc.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/asl_ipc.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/asl_ipcUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/asl_ipcServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/asl_ipcServer.c
    NOVOUCHERS)

add_darwin_static_library(syslog_asl_static)

target_sources(syslog_asl_static PRIVATE
    libsystem_asl.tproj/src/asl.c
    libsystem_asl.tproj/src/asl_client.c
    libsystem_asl.tproj/src/asl_core.c
    libsystem_asl.tproj/src/asl_fd.c
    libsystem_asl.tproj/src/asl_file.c
    libsystem_asl.tproj/src/asl_legacy1.c
    libsystem_asl.tproj/src/asl_msg.c
    libsystem_asl.tproj/src/asl_msg_list.c
    libsystem_asl.tproj/src/asl_object.c
    libsystem_asl.tproj/src/asl_store.c
    libsystem_asl.tproj/src/asl_string.c
    libsystem_asl.tproj/src/asl_util.c
    libsystem_asl.tproj/src/syslog.c
    pd_asl_mt_shim.c
    ${CMAKE_BINARY_DIR}/src/Libraries/syslog/asl_ipcUser.c
)

target_include_directories(syslog_asl_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libsystem_asl.tproj/include
)

target_include_directories(syslog_asl_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/aslcommon
    # membership.h + os/activity.h stand-ins for APIs with no OSS source here
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_trace/os-include   # os/log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include # libkern/OSAtomic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Kernel/xnu/libkern        # os/log_private.h + firehose/
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale          # xlocale_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD  # setlocale.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD # timelocal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime # Block.h
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/notify
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/os
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CrashReporterClient/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_compile_definitions(syslog_asl_static PRIVATE
    __PUREDARWIN__
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
)

target_compile_definitions(syslog_asl_static PRIVATE PUREDARWIN_ASL_NO_ACL=1
    PUREDARWIN_ASL_SYSLOG_BOTH_SPELLINGS=1)

target_compile_options(syslog_asl_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
    -w)

target_link_libraries(syslog_asl_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)

add_dependencies(syslog_asl_static libc_features)
