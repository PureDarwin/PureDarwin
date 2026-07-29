# TODO: Build libsystem_platform.dylib here.

add_library(libplatform_headers INTERFACE)
target_include_directories(libplatform_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(libplatform_private_headers INTERFACE)
target_include_directories(libplatform_private_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private)

# Static archive of the freestanding libplatform routines dyld needs when linked
# -nostdlib: the string/mem primitives (memcpy/memset/memmove/strlen/strcmp/...),
# the _simple_* buffered-I/O helpers, OSAtomic, and os_unfair_lock. Arch-specific
# atomics follow PUREDARWIN_ARCH.
if(PUREDARWIN_ARM64)
    # The upstream ARM64 file only contains the commpage FIFO entry points.
    # PureDarwin's merged libSystem also needs the ordinary OSAtomic API used
    # by libc and malloc; keep those portable ARM implementations in the
    # archive as well.
    set(_libplatform_atomics
        src/os/atomic.c
        src/atomics/arm64/OSAtomic.c)
    # ARM64 supplies the complete setjmp/longjmp implementation in one native
    # assembly file.  Its signal trampoline is the generic C implementation.
    set(_libplatform_setjmp
        src/setjmp/arm64/setjmp.s
        src/setjmp/generic/sigtramp.c)
else()
    # OSAtomic.s: OSAtomicAdd/CompareAndSwap primitives. Plus the PFZ (pre-emption-
    # free zone) commpage pieces dyld's OSAtomicFifoEnqueue/Dequeue reference:
    # pfz.s defines _preempt/_backoff, init.c defines the commpage_pfz_base pointer.
    set(_libplatform_atomics
        src/atomics/x86_64/OSAtomic.s
        src/atomics/x86_64/pfz.s
        src/atomics/init.c)
    set(_libplatform_setjmp
        src/setjmp/x86_64/setjmp.s
        src/setjmp/x86_64/_setjmp.s
        src/setjmp/x86_64/_sigtramp.s
        src/setjmp/generic/sigtramp.c)
endif()

add_darwin_static_library(libplatform_static)
target_sources(libplatform_static PRIVATE
    src/string/generic/bzero.c
    src/string/generic/ffsll.c
    src/string/generic/flsll.c
    src/string/generic/flsl.c
    src/string/generic/memccpy.c
    src/string/generic/memchr.c
    src/string/generic/memcmp.c
    src/string/generic/memmove.c
    src/string/generic/memset_pattern.c
    src/string/generic/strchr.c
    src/string/generic/strcmp.c
    src/string/generic/strcpy.c
    src/string/generic/strlcat.c
    src/string/generic/strlcpy.c
    src/string/generic/strlen.c
    src/string/generic/strncmp.c
    src/string/generic/strncpy.c
    src/string/generic/strnlen.c
    src/string/generic/strstr.c
    src/simple/getenv.c
    src/simple/asl.c
    src/simple/string_io.c
    src/os/lock.c
    src/os/alloc_once.c
    src/os/semaphore.c
    src/setjmp/generic/sigaction.c   # __platform_sigaction (libc's sigaction.c wrapper)
    ${_libplatform_setjmp}
    ${_libplatform_atomics}
)
# VARIANT_STATIC: emit the plain C names (memcpy/memmove/strlen/strcmp/...) as
# real symbols, not just the __platform_* internals -- this is the static-archive
# build variant, exactly what a -nostdlib client like dyld links against.
target_compile_definitions(libplatform_static PRIVATE
    VARIANT_STATIC
    PRIVATE=1
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
)
target_include_directories(libplatform_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os/resolver
    # os/tsd.h, gethostuuid.h and the userspace mach/* headers (vm_page_size.h)
    # live in libsystem_kernel.
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/os
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/private_include  # os/tsd.h (os/*.c)
    # The userspace mach/* headers (vm_page_size.h, vm_map.h). NB: include/mach/*
    # are broken symlinks; the real headers live under mach/mach/, so add mach/.
    ${CMAKE_CURRENT_SOURCE_DIR}/../libsystem_kernel/mach
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include  # generated MIG mach/*
    # reconstructed os/alloc_once_private.h (shared with pthread_static)
    ${CMAKE_CURRENT_SOURCE_DIR}/../pthread/compat-include
)
if(TARGET libsystem_kernel_static)
    add_dependencies(libplatform_static libsystem_kernel_static)
endif()
if(TARGET libc_headers_gen_script)
    add_dependencies(libplatform_static libc_headers_gen_script)
endif()
# bridgeos availability shim, force-included (C only -- no .s).
target_compile_options(libplatform_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/pd_bridgeos_compat.h>
)
target_link_libraries(libplatform_static PRIVATE
    AvailabilityHeaders
    architecture_headers        # architecture/i386/asm_help.h (OSAtomic.s)
    clang_bootstrap_headers
    libc_public_headers
    libc_private_headers
    pthread_common_headers
    pthread_common_private_headers
    # xnu_private_headers FIRST: the public mach/thread_switch.h strips
    # SWITCH_OPTION_OSLOCK_* (#ifdef PRIVATE); the System PrivateHeaders copy
    # keeps them and must win the <mach/thread_switch.h> lookup (lock.c).
    xnu_private_headers
    xnu_headers
)
set_source_files_properties(
    src/atomics/x86_64/OSAtomic.s
    src/atomics/x86_64/pfz.s
    PROPERTIES LANGUAGE ASM COMPILE_FLAGS "-x assembler-with-cpp"
)
# init.c assigns the commpage PFZ base (uintptr_t) to a void* -- width-preserving
# on LP64; the promoted -Werror=int-conversion is spurious here.
set_source_files_properties(src/atomics/init.c
    PROPERTIES COMPILE_FLAGS "-Wno-int-conversion")

install(
    FILES include/setjmp.h include/string_x86.h include/ucontext.h
    DESTINATION usr/include COMPONENT DeveloperTools
)

install_manpage(man/atomic.3 COMPONENT DeveloperTools ALIASES OSAtomicDequeue.3 OSAtomicEnqueue.3)
install_manpage(man/cache.3 COMPONENT DeveloperTools ALIASES sys_cache_control.3 sys_icache_invalidate.3 sys_dcache_flush.3)
install_manpage(man/ffs.3 COMPONENT DeveloperTools ALIASES ffsl.3 ffsll.3 fls.3 flsl.3 flsll.3)
install_manpage(man/getcontext.3 COMPONENT DeveloperTools ALIASES setcontext.3)
install_manpage(man/makecontext.3 COMPONENT DeveloperTools ALIASES swapcontext.3)
install_manpage(man/setjmp.3 COMPONENT DeveloperTools ALIASES _longjmp.3 _setjmp.3 longjmp.3 longjmperr.3 longjmperror.3 siglongjmp.3 sigsetjmp.3)
install_manpage(man/spinlock_deprecated.3 COMPONENT DeveloperTools ALIASES OSSpinLockLock.3 OSSpinLockTry.3 OSSpinLockUnlock.3)
install_manpage(man/ucontext.3 COMPONENT DeveloperTools)

install_manpage(man/atomic_deprecated.3 COMPONENT DeveloperTools ALIASES
    OSAtomicAdd32.3 OSAtomicAdd32Barrier.3 OSAtomicAdd64.3 OSAtomicAdd64Barrier.3
    OSAtomicAnd32.3 OSAtomicAnd32Barrier.3 OSAtomicAnd32Orig.3 OSAtomicAnd32OrigBarrier.3
    OSAtomicCompareAndSwap32.3 OSAtomicCompareAndSwap32Barrier.3 OSAtomicCompareAndSwap64.3
    OSAtomicCompareAndSwap64Barrier.3 OSAtomicCompareAndSwapInt.3 OSAtomicCompareAndSwapIntBarrier.3
    OSAtomicCompareAndSwapLong.3 OSAtomicCompareAndSwapLongBarrier.3 OSAtomicCompareAndSwapPtr.3
    OSAtomicCompareAndSwapPtrBarrier.3 OSAtomicDecrement32.3 OSAtomicDecrement32Barrier.3
    OSAtomicDecrement64.3 OSAtomicDecrement64Barrier.3 OSAtomicIncrement32.3 OSAtomicIncrement32Barrier.3
    OSAtomicIncrement64.3 OSAtomicIncrement64Barrier.3 OSAtomicOr32.3 OSAtomicOr32Barrier.3 OSAtomicOr32Orig.3
    OSAtomicOr32OrigBarrier.3 OSAtomicTestAndClear.3 OSAtomicTestAndClearBarrier.3 OSAtomicTestAndSet.3
    OSAtomicTestAndSetBarrier.3 OSAtomicXor32.3 OSAtomicXor32Barrier.3 OSAtomicXor32Orig.3 OSAtomicXor32OrigBarrier.3
    OSMemoryBarrier.3
)

install_manpage(man/stdatomic.3 COMPONENT DeveloperTools ALIASES
    ATOMIC_VAR_INIT.3 atomic_compare_exchange_strong.3
    atomic_compare_exchange_strong_explicit.3 atomic_compare_exchange_weak.3
    atomic_compare_exchange_weak_explicit.3 atomic_exchange.3 atomic_exchange_explicit.3
    atomic_fetch_add.3 atomic_fetch_add_explicit.3 atomic_fetch_and.3 atomic_fetch_and_explicit.3
    atomic_fetch_or.3 atomic_fetch_or_explicit.3 atomic_fetch_sub.3 atomic_fetch_sub_explicit.3 atomic_fetch_xor.3
    atomic_fetch_xor_explicit.3 atomic_init.3 atomic_is_lock_free.3 atomic_load.3 atomic_load_explicit.3 atomic_store.3
    atomic_store_explicit.3
)
