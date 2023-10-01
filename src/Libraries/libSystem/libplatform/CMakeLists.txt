# TODO: Build libsystem_platform.dylib here.

add_library(libplatform_headers INTERFACE)
target_include_directories(libplatform_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(libplatform_private_headers INTERFACE)
target_include_directories(libplatform_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private)

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
