add_darwin_static_library(IOKit)

mig(${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/device/device.defs
    INCLUDES_FROM_TARGET IOKit
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/device_user.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
    NOVOUCHERS)

target_sources(IOKit PRIVATE
    IOKitLib.c
    ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
)

target_compile_definitions(IOKit PRIVATE IOKIT=1 KERNEL=0)
target_compile_options(IOKit PRIVATE -UKERNEL)

target_include_directories(IOKit
    PUBLIC include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
        ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
)

target_link_libraries(IOKit PUBLIC
    xnu_headers
    xnu_private_headers
    libc_public_headers
    libsystem_kernel_static
    clang_bootstrap_headers
)

install(TARGETS IOKit DESTINATION usr/local/lib COMPONENT DeveloperInternal)
install(FILES include/PDIOKitLib.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
install(FILES include/IOKit/IOKitLib.h DESTINATION usr/local/include/IOKit COMPONENT DeveloperInternal)

if(PUREDARWIN_ENABLE_IOKITCF)

add_darwin_static_library(IOKitCF)

target_sources(IOKitCF PRIVATE
    iokituser/IOKitLib.c
    # iokit_user_client_trap is not in xnu's mach/syscall_sw.h, which is why
    # Apple ships the trap stub with IOKitUser rather than with libsyscall.
    iokituser/IOTrap.s
    iokituser/IOCFSerialize.c
    iokituser/IOCFUnserialize.tab.c
    ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
)

target_compile_definitions(IOKitCF PRIVATE IOKIT=1 KERNEL=0
    __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1)
target_compile_options(IOKitCF PRIVATE -UKERNEL -fblocks -w
    -Wno-error=int-conversion)

target_include_directories(IOKitCF
    PUBLIC iokituser/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/iokituser/pd-compat-include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/darwin
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/locale
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/locale/FreeBSD
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/stdtime/FreeBSD
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/pd-compat-include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libplatform/include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libplatform/private
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/pthread/include/pthread
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/pthread/private
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/private_include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
        ${CMAKE_SOURCE_DIR}/src/Libraries/libsystem_trace/os-include
        ${CMAKE_SOURCE_DIR}/src/Libraries/syslog/libsystem_asl.tproj/include  # asl.h
        ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/libxpc/include
        ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/libinfo/compat  # dispatch/private.h
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime
        ${PUREDARWIN_COREFOUNDATION_PREFIX}/include
        ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/iokit
        ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
        ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
        ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_compile_options(IOKitCF PRIVATE
    -F${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks)

target_link_libraries(IOKitCF PUBLIC
    xnu_headers
    xnu_private_headers
    libc_public_headers
    libsystem_kernel_static
    clang_bootstrap_headers
    AvailabilityHeaders
    libc_features
    pthread_common_headers
)

add_dependencies(IOKitCF libc_features)

install(TARGETS IOKitCF DESTINATION usr/local/lib COMPONENT DeveloperInternal)

endif()