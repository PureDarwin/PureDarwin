mig(notify_ipc.defs
    INCLUDES_FROM_TARGET XPC_libxpc_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/notify_ipc.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/notify_ipc_user.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/notify_ipcServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/notify_ipcServer.c
    NOVOUCHERS)

add_library(XPC_notify_client_headers INTERFACE)
target_include_directories(XPC_notify_client_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

set(PD_NOTIFY_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../libinfo/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../launchd/System-compat
    ${CMAKE_BINARY_DIR}/src/Libraries/XPC/launchd
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libinfo
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/mach
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/os
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../CrashReporterClient/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/pd-compat-include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
)

add_darwin_static_library(XPC_notify_client_static)
target_sources(XPC_notify_client_static PRIVATE
    notify_client.c
    libnotify.c
    table.c
    pd_xpc_event_publisher.c
)
target_compile_definitions(XPC_notify_client_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
target_compile_options(XPC_notify_client_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/pd_notifyd_compat.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include)
target_include_directories(XPC_notify_client_static PRIVATE ${PD_NOTIFY_INCLUDES})
target_link_libraries(XPC_notify_client_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(XPC_notify_client_static libc_features libsystem_kernel_static)

add_darwin_static_library(XPC_notifyd_mig_static)
target_sources(XPC_notifyd_mig_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/notify_ipc_user.c
    ${CMAKE_CURRENT_BINARY_DIR}/notify_ipcServer.c
)
target_compile_definitions(XPC_notifyd_mig_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)
target_compile_options(XPC_notifyd_mig_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include)
target_include_directories(XPC_notifyd_mig_static PRIVATE ${PD_NOTIFY_INCLUDES})
add_dependencies(XPC_notifyd_mig_static libsystem_kernel_static)
target_link_libraries(XPC_notifyd_mig_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)

add_darwin_executable(notifyd MACOSX_VERSION_MIN 11.0)

get_target_property(_notifyd_opts notifyd COMPILE_OPTIONS)
if(_notifyd_opts)
    list(REMOVE_ITEM _notifyd_opts -nostdinc)
    set_target_properties(notifyd PROPERTIES COMPILE_OPTIONS "${_notifyd_opts}")
endif()

target_sources(notifyd PRIVATE
    notifyd.c
    notify_proc.c
    pathwatch.c
    service.c
    pd_sandbox_check.c
)
target_compile_definitions(notifyd PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
target_compile_options(notifyd PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/pd_notifyd_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-includeerrno.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include
    -ferror-limit=0)
target_include_directories(notifyd PRIVATE ${PD_NOTIFY_INCLUDES})
target_link_options(notifyd PRIVATE
    -nostdlib
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -Wl,-force_load,$<TARGET_FILE:XPC_notifyd_mig_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_notify_client_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libxpc_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libinfo_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libnv_static>
    -Wl,-force_load,$<TARGET_FILE:CrashReporterClient>
    -lSystem
    $<TARGET_FILE:XPC_launchd_mig_static>
)
add_dependencies(notifyd
    XPC_launchd_mig_static
    libSystem_B_stub
    dyld
    CrashReporterClient
    XPC_notify_client_static
    XPC_notifyd_mig_static
    XPC_libxpc_static
    XPC_libinfo_static
    XPC_libnv_static
    XPC_launchd_mig_static
)

install(TARGETS notifyd DESTINATION usr/sbin COMPONENT BaseSystem OPTIONAL)
install(FILES com.apple.notifyd.plist DESTINATION System/Library/LaunchDaemons COMPONENT BaseSystem OPTIONAL)
