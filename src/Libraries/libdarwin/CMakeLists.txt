set(_libdarwin_dir ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/libdarwin)

add_darwin_static_library(libdarwin_static)

target_sources(libdarwin_static PRIVATE
    ${_libdarwin_dir}/bsd.c
    ${_libdarwin_dir}/ctl.c
    ${_libdarwin_dir}/err.c
    ${_libdarwin_dir}/exception.c
    ${_libdarwin_dir}/init.c
    ${_libdarwin_dir}/mach.c
    ${_libdarwin_dir}/stdio.c
    ${_libdarwin_dir}/stdlib.c
    ${_libdarwin_dir}/string.c
    ${_libdarwin_dir}/variant.c
)

target_include_directories(libdarwin_static PUBLIC
    # h/*.h are installed as <os/*.h>.
    ${_libdarwin_dir}/os-include
)

target_include_directories(libdarwin_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-compat-include
    ${_libdarwin_dir}
    # xnu's bsd/sys/csr.h is installed as <System/sys/csr.h>.
    ${_libdarwin_dir}/sys-include
    # os/assumes.h's os_crash() variant includes <os/log_private.h>.
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/pd-compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin
    ${_libdarwin_dir}/h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/locale/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/stdtime/FreeBSD
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch          # os/object_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libinfo/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libdispatch/src/BlocksRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/private  # os/lock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libmalloc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libmalloc/compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/pthread/compat-include  # os/alloc_once_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/mach
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../XPC/libxpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dyld/upstream/include          # dlfcn.h
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd                        # sys/guarded.h, uuid/uuid.h
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_compile_definitions(libdarwin_static PRIVATE
    __PUREDARWIN__
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    PRIVATE=1
    OS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE=1
)

target_compile_options(libdarwin_static PRIVATE
    -fblocks
    # pd_bridgeos_compat.h must be force-included BEFORE the prelude: the
    # prelude pulls headers that use __API_AVAILABLE(...bridgeos(x)...).
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/pd_libdarwin_prelude.h>
    -w)

target_link_libraries(libdarwin_static PRIVATE
    architecture_headers          # architecture/byte_order.h (mach-o/loader.h)
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
)

add_dependencies(libdarwin_static libc_features libsystem_kernel_static)
