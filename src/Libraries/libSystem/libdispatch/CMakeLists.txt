add_darwin_static_library(libdispatch_static)
mig(src/protocol.defs INCLUDES_FROM_TARGET libdispatch_static
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/protocol.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/protocol.c
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/protocolServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/protocolServer.c
    NOVOUCHERS)

target_sources(libdispatch_static PRIVATE
    src/allocator.c
    src/apply.c
    src/benchmark.c
    src/block.cpp
    src/data.c
    # data.m: OS_dispatch_data / OS_dispatch_data_empty ObjC classes
    # (USE_OBJC=1), same "C file + matching .m" split as object.c/object.m.
    src/data.m
    src/init.c
    src/introspection.c
    src/io.c
    src/mach.c
    # object.c: core refcounting + public dispatch_retain/release/etc (uses
    # DISPATCH_OBJECT_TFB to bridge to object.m's ObjC classes below when
    # applicable). object.m: the ObjC OS_object class hierarchy itself
    # (OS_OBJECT_USE_OBJC=1). Real Apple compiles both together, not one or
    # the other.
    src/object.c
    src/object.m
    src/once.c
    src/queue.c
    src/semaphore.c
    src/source.c
    src/time.c
    src/transform.c
    src/voucher.c
    src/workgroup.c
    src/eventlink.c
    src/shims.c
    src/event/event.c
    src/event/event_kevent.c
    src/shims/lock.c
    src/shims/yield.c
    src/BlocksRuntime/data.c
    src/BlocksRuntime/runtime.c
    ${CMAKE_CURRENT_BINARY_DIR}/protocol.c
    ${CMAKE_CURRENT_BINARY_DIR}/protocolServer.c
    compat-include/pd_libdispatch_link_gaps.c
)
set_target_properties(libdispatch_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_source_files_properties(src/object.m src/data.m PROPERTIES
    COMPILE_OPTIONS "-fno-objc-arc;-fobjc-exceptions")

target_compile_definitions(libdispatch_static PRIVATE
    __PUREDARWIN__=1
    # Must match libc_static and libsystem_kernel_static (see the note in
    # libsystem_kernel/CMakeLists.txt): without it, <unistd.h> asm-renames
    # close/read/pread/pwrite to their $UNIX2003 spellings, which do not exist
    # on x86_64 - nothing referenced them until asl_fd.c pulled in io.c.
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    DISPATCH_USE_INTERNAL_WORKQUEUE=0
    HAVE_OBJC=1
    USE_OBJC=1
    OS_OBJECT_USE_OBJC=1
    DISPATCH_USE_DTRACE=0
    VOUCHER_USE_MACH_VOUCHER=0
    PRIVATE=1
    __PTHREAD_EXPOSE_INTERNALS__=1
    OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY=1
    DISPATCH_VARIANT_STATIC=1
)
target_include_directories(libdispatch_static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    # object.m / object_internal.h (USE_OBJC=1): <objc/objc-exception.h>,
    # <Foundation/NSString.h>
    ${CMAKE_BINARY_DIR}/generated-headers
    ${CMAKE_CURRENT_SOURCE_DIR}/compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    # pthread/private.h, tsd_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/private_include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    # event_kevent.c wants BSD's sys/event.h, sys/queue.h.
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/include
    # mach_get_times() (mach/mach_time.h - libc/include's copy lacks it,
    # only xnu's own osfmk one declares it) and libproc.h (proc_pidinfo).
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
)
target_link_libraries(libdispatch_static PRIVATE
    AvailabilityHeaders
    architecture_headers
    clang_bootstrap_headers
    libc_public_headers
    libc_private_headers
    libplatform_headers
    libplatform_private_headers
    xnu_private_headers
    xnu_headers)
if(TARGET pthread_common_headers)
    target_link_libraries(libdispatch_static PRIVATE pthread_common_headers pthread_common_private_headers)
endif()
target_compile_options(libdispatch_static PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -Wno-availability
    -include${CMAKE_CURRENT_SOURCE_DIR}/compat-include/pd_neuter_api_available.h
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/compat-include/pd_bridgeos_compat_cxx.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_SOURCE_DIR}/src/Libraries/dyld/dyld_sdk_compat.h>
    -include${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd/sys_private/kdebug_private.h
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern/os/base_private.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/compat-include/pd_work_interval_instance.h>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17;-fblocks;-fno-rtti;-fno-exceptions;-isystem${PUREDARWIN_MACOSX_SDK}/usr/include/c++/v1;-isystem${PUREDARWIN_MACOSX_SDK}/usr/include>)

add_dependencies(libdispatch_static libsystem_kernel_static)

target_include_directories(libdispatch_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libc/include)

target_include_directories(libdispatch_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/mach)

target_include_directories(libdispatch_static PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern)

target_include_directories(libdispatch_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libc/stdtime/FreeBSD)

target_include_directories(libdispatch_static PRIVATE
    ${CMAKE_BINARY_DIR}/generated-headers/dlfcn)

