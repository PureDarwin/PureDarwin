add_darwin_static_library(IOKit)

# device.defs gates the 32-bit io_connect_map_memory routines on __ILP32__,
# which mig only defines if it preprocesses for the real target arch rather
# than its x86_64 default.
if(PUREDARWIN_ARCH STREQUAL "armv6")
    set(_iokit_mig_arch armv6)
else()
    set(_iokit_mig_arch x86_64)
endif()

mig(${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/device/device.defs
    INCLUDES_FROM_TARGET IOKit
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/device_user.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
    ARCH ${_iokit_mig_arch}
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
    iokituser/IOPowerSources.c
    iokituser/IOPMLib.c
    # CFPlugIn-backed device access, and the shared-memory queue clients read
    # events from. (ionotify.c/ionotifyCB.c are sample programs, not library
    # sources; the notification API is already in IOKitLib.c.)
    iokituser/IOCFPlugIn.c
    iokituser/pd_oskext_folders.c
    iokituser/IODataQueueClient.c
    iokituser/IOMIGMachPort.c
    # IOHIDLib's C API. The IOHIDEventSystem*/IOHIDService*/IOHIDSession* half
    # of hid.subproj ships as empty files in the drop, IOHIDPlugins.c included.
    iokituser/hid.subproj/IOHIDManager.c
    iokituser/hid.subproj/IOHIDDevice.c
    iokituser/hid.subproj/IOHIDElement.c
    iokituser/hid.subproj/IOHIDQueue.c
    iokituser/hid.subproj/IOHIDTransaction.c
    iokituser/hid.subproj/IOHIDValue.c
    iokituser/hid.subproj/IOHIDLibPrivate.c
    iokituser/hid.subproj/IOHIDPreferences.c
    iokituser/hid.subproj/pd_hid_reconstructed.c
    # IOHIDDevice/IOHIDElement are CF-bridged ObjC objects upstream: their
    # structs start with objc_object and the constructors alloc a real class.
    iokituser/hid.subproj/HIDDeviceBase.m
    iokituser/hid.subproj/HIDElementBase.m
    iokituser/hid.subproj/IOHIDDeviceObjc.m
    iokituser/hid.subproj/IOHIDElementObjc.m
    ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
)

# IOCFPlugIn.c is wholly inside #ifdef HAVE_CFPLUGIN; our CoreFoundation has
# the CFPlugIn/CFBundle surface it needs.
# hid.subproj calls the os_assert(expr, fmt, ...) form, which only exists on
# assumes.h's libtrace path; _os_crash_fmt backs it in pd_assumes.c.
target_compile_definitions(IOKitCF PRIVATE IOKIT=1 KERNEL=0
    __DARWIN_ONLY_UNIX_CONFORMANCE=1 PRIVATE=1 HAVE_CFPLUGIN=1
    OS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE=1)
target_compile_options(IOKitCF PRIVATE -UKERNEL -fblocks -w
    -Wno-error=int-conversion
    # hid.subproj's ObjC is MRR, not ARC.
    $<$<COMPILE_LANGUAGE:OBJC>:-fno-objc-arc>)

target_include_directories(IOKitCF
    PUBLIC iokituser/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/iokituser
        ${CMAKE_CURRENT_SOURCE_DIR}/iokituser/hid.subproj
        ${CMAKE_CURRENT_SOURCE_DIR}/iokituser/include/IOKit/hid
        ${CMAKE_CURRENT_SOURCE_DIR}/iokituser/pd-compat-include
        # IOHIDKeys.h/IOHIDUsageTables.h/IOHIDLibUserClient.h are shared with
        # the kext and KERNEL-guarded. Listed after iokituser/include so the
        # user-space IOHIDDevice.h wins over the kernel class of that name.
        ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOHIDFamily/include
        ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOHIDFamily/IOHIDSystem
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
        # NSObject for the HIDDevice/HIDElement classes, NSString for their
        # -description.
        ${PUREDARWIN_LIBOBJC_PREFIX}/usr/include
        ${PUREDARWIN_FOUNDATION_PREFIX}/usr/include
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