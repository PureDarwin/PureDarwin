set(PD_LOGD_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libinfo
    ${CMAKE_CURRENT_SOURCE_DIR}/../launchd
    ${CMAKE_CURRENT_SOURCE_DIR}/../launchd/System-compat
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

add_darwin_executable(logd MACOSX_VERSION_MIN 11.0)

get_target_property(_logd_opts logd COMPILE_OPTIONS)
if(_logd_opts)
    list(REMOVE_ITEM _logd_opts -nostdinc)
    set_target_properties(logd PROPERTIES COMPILE_OPTIONS "${_logd_opts}")
endif()

target_sources(logd PRIVATE
    logd.c
)
target_compile_definitions(logd PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
target_compile_options(logd PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libplatform/pd_bridgeos_compat.h>
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -I${PUREDARWIN_MACOSX_SDK}/usr/include
    -ferror-limit=0)
target_include_directories(logd PRIVATE ${PD_LOGD_INCLUDES})
target_link_options(logd PRIVATE
    -nostdlib
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -Wl,-force_load,$<TARGET_FILE:XPC_libxpc_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libinfo_static>
    -Wl,-force_load,$<TARGET_FILE:XPC_libnv_static>
    -Wl,-force_load,$<TARGET_FILE:CrashReporterClient>
    -lSystem
    $<TARGET_FILE:XPC_launchd_mig_static>
)
add_dependencies(logd
    XPC_launchd_mig_static
    libSystem_B_stub
    dyld
    CrashReporterClient
    XPC_libxpc_static
    XPC_libinfo_static
    XPC_libnv_static
)

install(TARGETS logd DESTINATION usr/libexec COMPONENT BaseSystem OPTIONAL)
install(FILES com.apple.logd.plist DESTINATION System/Library/LaunchDaemons COMPONENT BaseSystem OPTIONAL)
