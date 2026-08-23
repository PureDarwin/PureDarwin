/*
 * ____chkstk_darwin: Clang emits a call to this before any function whose
 * stack frame exceeds a page, to probe (touch) each page of the
 * to-be-used stack space so a guard-page-based lazily-committed stack
 * grows safely instead of skipping straight past its guard page.
 *
 * XNU/pthread stacks here are fixed-size and fully mapped at thread
 * creation (no lazy guard-page growth to protect), so there is nothing
 * to probe - this only needs to satisfy the calling convention (requested
 * size in %rax, every other register including flags preserved, no
 * stack frame of its own) and return.
 */
    .text
    .globl ____chkstk_darwin
____chkstk_darwin:
#if defined(__arm__)
    bx lr
#else
    /* arm64 and x86 both return with ret; only arm32 uses bx lr. */
    ret
#endif
