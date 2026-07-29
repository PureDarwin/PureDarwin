if(NOT PUREDARWIN_ENABLE_DISKARBITRATION)
    return()
endif()

if(NOT PUREDARWIN_COREFOUNDATION_PREFIX AND DEFINED ENV{PUREDARWIN_COREFOUNDATION_PREFIX})
    set(PUREDARWIN_COREFOUNDATION_PREFIX "$ENV{PUREDARWIN_COREFOUNDATION_PREFIX}")
endif()
if(NOT PUREDARWIN_COREFOUNDATION_PREFIX)
    message(FATAL_ERROR "PUREDARWIN_COREFOUNDATION_PREFIX must point to the CoreFoundation install prefix")
endif()

add_darwin_shared_library(DiskArbitration
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /System/Library/Frameworks/DiskArbitration.framework/Versions/A/DiskArbitration)

# Only the client side: diskarbitrationd is the other end and is not built here.
mig(diskarbitrationd/DAServer.defs
    INCLUDES_FROM_TARGET DiskArbitration
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/DAServer.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/DAServerUser.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/DAServerServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/DAServerServer.c
    ARCH ${PUREDARWIN_ARCH}
    NOVOUCHERS)

target_sources(DiskArbitration PRIVATE
    DiskArbitration/DADisk.c
    DiskArbitration/DADissenter.c
    DiskArbitration/DASession.c
    DiskArbitration/DiskArbitration.c
    DiskArbitration/DiskArbitrationPrivate.c
    # DiskArbitration/DAInternal.c is a one-line wrapper that #includes the
    # daemon's copy; build that directly instead of compiling both.
    diskarbitrationd/DAInternal.c
    ${CMAKE_CURRENT_BINARY_DIR}/DAServerUser.c
)

target_include_directories(DiskArbitration PUBLIC include)

target_include_directories(DiskArbitration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/DiskArbitration
    ${CMAKE_CURRENT_SOURCE_DIR}/diskarbitrationd
    ${CMAKE_CURRENT_BINARY_DIR}                                  # DAServer.h (MIG)
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_trace/os-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../CrashReporterClient/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Security/include
    # Apple's CoreFoundation-based IOKitLib (IOKitUser), not PureDarwin's
    # CF-free PDIOKitLib - DA needs IOBSDNameMatching, the registry property
    # calls and kIOServicePlane.
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOKit/iokituser/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dyld/upstream/include              # mach-o/dyld.h
    # libdarwin's forwarder, NOT xnu/EXTERNAL_HEADERS directly: that directory's
    # AvailabilityInternal.h is the kernel copy and shadows AvailabilityVersions',
    # silently breaking __API_AVAILABLE.
    ${CMAKE_SOURCE_DIR}/src/Libraries/libdarwin/pd-compat-include      # mach-o/loader.h
    ${CMAKE_SOURCE_DIR}/src/Libraries/architecture/include             # architecture/byte_order.h
    # IOMedia/IOBSD plus the CD/DVD/BD media class names. Their C++ driver
    # classes sit behind #ifdef KERNEL, so userspace sees only the constants.
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOStorageFamily/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOCDStorageFamily/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IODVDStorageFamily/include
    ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOBDStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/iokit
)

target_compile_definitions(DiskArbitration PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
)

target_compile_options(DiskArbitration PRIVATE
    -fblocks
    -F${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
    -w
    # DiskArbitration.c passes void * straight to mach_vm_offset_t parameters -
    # valid on Darwin, and only a warning to the compiler Apple built this with.
    -Wno-error=int-conversion
    # DAInternal.h declares its kDA* constants as tentative definitions, so every
    # translation unit that includes it emits one. That is fine under the -fcommon
    # semantics Apple built this with; modern clang defaults to -fno-common and
    # turns them into 155 duplicate symbols.
    -fcommon)

target_link_options(DiskArbitration PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-platform_version,macos,11.0,11.5
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation"
    $<TARGET_FILE:CrashReporterClient>
    -Wl,-L,${PUREDARWIN_IOKIT_PREFIX}/usr/lib
    -lIOKitCF
    "${PUREDARWIN_SECURITY_PREFIX}/System/Library/Frameworks/Security.framework/Versions/A/Security"
    -lSystem)

target_link_libraries(DiskArbitration PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

add_dependencies(DiskArbitration libc_features libSystem_B_stub dyld libsystem_kernel_static CrashReporterClient)

install(TARGETS DiskArbitration
    DESTINATION System/Library/Frameworks/DiskArbitration.framework/Versions/A
    COMPONENT BaseSystem)
