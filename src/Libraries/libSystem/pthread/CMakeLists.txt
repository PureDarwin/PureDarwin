add_library(pthread_common_headers INTERFACE)
target_include_directories(pthread_common_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(pthread_common_private_headers INTERFACE)
target_include_directories(pthread_common_private_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private)

add_darwin_static_library(pthread_static)
set_target_properties(pthread_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(pthread_static PRIVATE
    src/pthread.c
    src/pthread_mutex.c
    src/pthread_cond.c          # pthread_cond_wait/broadcast/signal
    src/pthread_rwlock.c        # pthread_rwlock_{rd,wr}lock/unlock
    src/pthread_tsd.c
    src/pthread_cancelable.c    # __pthread_exit_if_canceled, cancellation points
    src/pthread_atfork.c
    src/pthread_dependency.c
    src/pthread_asm.s
    pd_main_qos.c
    pd_dyld_variant_extras.c    # _pthread_atomic_xchg_ptr + _pthread_qos_class_encode_workqueue
)
target_compile_definitions(pthread_static PRIVATE
    VARIANT_DYLD=1 __POSIX_LIB__ PRIVATE=1 __PTHREAD_EXPOSE_INTERNALS__=1
    PD_PTHREAD_TSD_DYLD_RANGE=1)
target_include_directories(pthread_static PRIVATE
    # Reconstructed Apple-internal os/alloc_once_private.h (absent from the open
    # libplatform drop). Must precede libplatform's private/ so the wrapper is found.
    ${CMAKE_CURRENT_SOURCE_DIR}/compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resolver
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pthread   # internal.h does #include "pthread.h"
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/mach
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    # platform/string.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libmalloc/private   # stack_logging.h
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/bsd        # pthread/bsdthread_private.h, pthread/workqueue_syscalls.h
)
target_link_libraries(pthread_static PRIVATE
    AvailabilityHeaders
    architecture_headers
    clang_bootstrap_headers
    libc_public_headers
    libc_private_headers
    libplatform_headers
    libplatform_private_headers
    xnu_private_headers
    xnu_headers
)
target_compile_options(pthread_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/pd_bridgeos_compat.h>
    -Wno-int-conversion
)
set_source_files_properties(src/pthread_asm.s PROPERTIES
    LANGUAGE ASM COMPILE_FLAGS "-x assembler-with-cpp -UVARIANT_DYLD")
set_source_files_properties(src/pthread_tsd.c PROPERTIES
    COMPILE_FLAGS "-UVARIANT_DYLD -DPTHREAD_KEY_LEGACY_SUPPORT=0")
# The generated mach MIG headers (public_include) must exist before we compile.
if(TARGET libsystem_kernel_static)
    add_dependencies(pthread_static libsystem_kernel_static)
endif()

add_darwin_static_library(pthread_full_static)
target_sources(pthread_full_static PRIVATE
    src/pthread.c
    src/pthread_mutex.c
    src/pthread_cond.c
    src/pthread_rwlock.c
    src/pthread_tsd.c
    src/pthread_cancelable.c
    src/pthread_atfork.c
    src/pthread_dependency.c
    src/pthread_asm.s
    src/qos.c                  # real _pthread_set_main_qos/_pthread_qos_class_encode_workqueue (pd_main_qos.c is the VARIANT_DYLD-only substitute)
)
target_compile_definitions(pthread_full_static PRIVATE
    __POSIX_LIB__ PRIVATE=1 __PTHREAD_EXPOSE_INTERNALS__=1
    # This target is merged into libSystem_B_stub alongside real libc_static
    # (unlike real Apple's separate libpthread.dylib/Libc.dylib split) - skip
    # pthread.c's own memset/memcpy/bzero shims, real libc's are already here.
    PD_PTHREAD_MERGED_INTO_LIBSYSTEM=1)
target_include_directories(pthread_full_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat-include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resolver
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/mach
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/include   # real spawn.h (posix_spawnattr_t) for qos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../libc                        # <os/variant_private.h> (libc_static uses this same source-root trick)
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/private
    ${CMAKE_CURRENT_SOURCE_DIR}/../libmalloc/private
    ${PUREDARWIN_SOURCE_DIR}/src/Kernel/xnu/bsd
)
target_link_libraries(pthread_full_static PRIVATE
    AvailabilityHeaders
    architecture_headers
    clang_bootstrap_headers
    libc_public_headers
    libc_private_headers
    libplatform_headers
    libplatform_private_headers
    xnu_private_headers
    xnu_headers
)
target_compile_options(pthread_full_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libplatform/pd_bridgeos_compat.h>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/include/spawn.h>
    -Wno-int-conversion
)
set_source_files_properties(src/pthread_asm.s PROPERTIES
    LANGUAGE ASM COMPILE_FLAGS "-x assembler-with-cpp -UVARIANT_DYLD")
if(TARGET libsystem_kernel_static)
    add_dependencies(pthread_full_static libsystem_kernel_static)
endif()
