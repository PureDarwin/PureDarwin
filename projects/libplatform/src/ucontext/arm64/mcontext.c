#include "asm_help.h"

#define _XOPEN_SOURCE 600L
#include <ucontext.h>
#include <stddef.h>

#include <mach/arm/thread_status.h>

_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[0]) == MCONTEXT_OFFSET_X0,
		"MCONTEXT_OFFSET_X0");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[19]) == MCONTEXT_OFFSET_X19_X20,
		"MCONTEXT_OFFSET_X19_X20");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[21]) == MCONTEXT_OFFSET_X21_X22,
		"MCONTEXT_OFFSET_X21_X22");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[23]) == MCONTEXT_OFFSET_X23_X24,
		"MCONTEXT_OFFSET_X23_X24");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[25]) == MCONTEXT_OFFSET_X25_X26,
		"MCONTEXT_OFFSET_X25_X26");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__x[27]) == MCONTEXT_OFFSET_X27_X28,
		"MCONTEXT_OFFSET_X27_X28");

#if __has_feature(ptrauth_calls)
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__opaque_fp) == MCONTEXT_OFFSET_FP_LR,
		"MCONTEXT_OFFSET_FP_LR");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__opaque_sp) == MCONTEXT_OFFSET_SP,
		"MCONTEXT_OFFSET_SP");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__opaque_flags) == MCONTEXT_OFFSET_FLAGS,
		"MCONTEXT_OFFSET_FLAGS");
#else
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__fp) == MCONTEXT_OFFSET_FP_LR,
		"MCONTEXT_OFFSET_FP_LR");
_Static_assert(offsetof(struct __darwin_mcontext64, __ss.__sp) == MCONTEXT_OFFSET_SP,
		"MCONTEXT_OFFSET_SP");
#endif


// Neon registers are 128 bits wide. d suffix refers to the last 64 bits of the
// 128 bit register. Hence the -8 offset.
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[8]) == (MCONTEXT_OFFSET_D8 - 8),
		"MCONTEXT_OFFSET_D8");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[9]) == (MCONTEXT_OFFSET_D9 - 8),
		"MCONTEXT_OFFSET_D9");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[10]) == (MCONTEXT_OFFSET_D10 - 8),
		"MCONTEXT_OFFSET_D10");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[11]) == (MCONTEXT_OFFSET_D11 - 8),
		"MCONTEXT_OFFSET_D11");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[12]) == (MCONTEXT_OFFSET_D12 - 8),
		"MCONTEXT_OFFSET_D12");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[13]) == (MCONTEXT_OFFSET_D13 - 8),
		"MCONTEXT_OFFSET_D13");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[14]) == (MCONTEXT_OFFSET_D14 - 8),
		"MCONTEXT_OFFSET_D14");
_Static_assert(offsetof(struct __darwin_mcontext64, __ns.__v[15]) == (MCONTEXT_OFFSET_D15 - 8),
		"MCONTEXT_OFFSET_D15");

#if __has_feature(ptrauth_calls)
_Static_assert((1 << LR_SIGNED_WITH_IB_BIT) == LR_SIGNED_WITH_IB, "LR_SIGNED_WITH_IB_BIT");
#endif
