set(libplatform_sources
    src/atomics/common/MKGetTimeBaseInfo.c
    src/atomics/init.c
    src/atomics/x86_64/pfz.s
    src/atomics/x86_64/OSAtomic.s
    src/cachecontrol/generic/cache.c
    src/cachecontrol/x86_64/cache.s
    src/introspection/introspection.c
    src/os/alloc_once.c
    src/os/atomic.c
    src/setjmp/x86_64/_setjmp.s
    src/setjmp/x86_64/_sigtramp.s
    src/setjmp/x86_64/setjmp.s
    src/setjmp/generic/setjmperr.c
    src/setjmp/generic/sigaction.c
    src/setjmp/generic/sigtramp.c
    src/simple/asl.c
    src/simple/getenv.c
    src/simple/string_io.c
    src/string/generic/bzero.c
    src/string/generic/ffsll.c
    src/string/generic/flsll.c
    src/string/generic/memchr.c
    src/string/generic/memcmp.c
    src/string/generic/memccpy.c
    src/string/generic/memmove.c
    src/string/generic/memset_pattern.c
    src/string/generic/strcmp.c
    src/string/generic/strcpy.c
    src/string/generic/strlen.c
    src/string/generic/strncmp.c
    src/string/generic/strncpy.c
    src/string/generic/strnlen.c
    src/string/generic/strchr.c
    src/string/generic/strstr.c
    src/ucontext/generic/getmcontext.c
    src/ucontext/generic/makecontext.c
    src/ucontext/generic/setcontext.c
    src/ucontext/generic/swapcontext.c
    src/ucontext/x86_64/getcontext.s
    src/ucontext/x86_64/_setcontext.s
    src/ucontext/x86_64/_ctx_start.s
    src/init.c
)

add_darwin_shared_library(libplatform NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
set_property(TARGET libplatform PROPERTY PREFIX lib)
set_property(TARGET libplatform PROPERTY OUTPUT_NAME system_platform)
target_sources(libplatform PRIVATE ${libplatform_sources})
target_link_libraries(libplatform PUBLIC libplatform_headers)
target_link_libraries(libplatform PRIVATE libplatform_private_headers)

#add_firstpass_library(libplatform)


add_darwin_static_library(libplatform_dyld)
target_sources(libplatform_dyld PRIVATE ${libplatform_sources})
target_compile_definitions(libplatform_dyld PRIVATE VARIANT_STATIC=1 VARIANT_DYLD=1)
target_link_libraries(libplatform_dyld PUBLIC libplatform_headers)
target_link_libraries(libplatform_dyld PRIVATE libplatform_private_headers)


add_library(libplatform_headers INTERFACE)
target_include_directories(libplatform_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(libplatform_private_headers INTERFACE)
target_include_directories(libplatform_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private)

install(
    FILES include/setjmp.h include/string_x86.h include/ucontext.h
    DESTINATION usr/include COMPONENT DeveloperTools
)

install_manpage(man/atomic.3 OSAtomicDequeue.3 OSAtomicEnqueue.3)
install_manpage(man/cache.3 sys_cache_control.3 sys_icache_invalidate.3 sys_dcache_flush.3)
install_manpage(man/ffs.3 ffsl.3 ffsll.3 fls.3 flsl.3 flsll.3)
install_manpage(man/getcontext.3 setcontext.3)
install_manpage(man/makecontext.3 swapcontext.3)
install_manpage(man/setjmp.3 _longjmp.3 _setjmp.3 longjmp.3 longjmperr.3 longjmperror.3 siglongjmp.3 sigsetjmp.3)
install_manpage(man/spinlock_deprecated.3 OSSpinLockLock.3 OSSpinLockTry.3 OSSpinLockUnlock.3)
install_manpage(man/ucontext.3)

install_manpage(man/atomic_deprecated.3
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

install_manpage(man/stdatomic.3 ATOMIC_VAR_INIT.3 atomic_compare_exchange_strong.3
    atomic_compare_exchange_strong_explicit.3 atomic_compare_exchange_weak.3
    atomic_compare_exchange_weak_explicit.3 atomic_exchange.3 atomic_exchange_explicit.3
    atomic_fetch_add.3 atomic_fetch_add_explicit.3 atomic_fetch_and.3 atomic_fetch_and_explicit.3
    atomic_fetch_or.3 atomic_fetch_or_explicit.3 atomic_fetch_sub.3 atomic_fetch_sub_explicit.3 atomic_fetch_xor.3
    atomic_fetch_xor_explicit.3 atomic_init.3 atomic_is_lock_free.3 atomic_load.3 atomic_load_explicit.3 atomic_store.3
    atomic_store_explicit.3
)
