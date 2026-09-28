// riscv darwin's c types and layout follow arm64 darwin, only architectural macros may differ
#define SAME(t, u) _Generic((t)0, u: 1, default: 0)
_Static_assert((char)-1 < 0, "plain char is signed");
_Static_assert((__WCHAR_TYPE__)-1 < 0 && sizeof(__WCHAR_TYPE__) == 4, "wchar_t is int");
_Static_assert(SAME(__WINT_TYPE__, int), "wint_t is int");
_Static_assert(SAME(__SIZE_TYPE__, unsigned long), "size_t is unsigned long");
_Static_assert(SAME(__PTRDIFF_TYPE__, long), "ptrdiff_t is long");
_Static_assert(SAME(__INTPTR_TYPE__, long), "intptr_t is long");
_Static_assert(SAME(__INTMAX_TYPE__, long), "intmax_t is long");
_Static_assert(SAME(__UINTMAX_TYPE__, unsigned long), "uintmax_t is unsigned long");
_Static_assert(SAME(__INT64_TYPE__, long long), "int64_t is long long");
_Static_assert(SAME(__INT_FAST64_TYPE__, long long) && SAME(__INT_LEAST64_TYPE__, long long), "fast/least 64");
_Static_assert(SAME(__INT_FAST8_TYPE__, signed char) && SAME(__INT_FAST16_TYPE__, short) && SAME(__INT_FAST32_TYPE__, int), "fast types");
_Static_assert(__SIG_ATOMIC_WIDTH__ == 32 && __SIG_ATOMIC_MAX__ == 2147483647, "sig_atomic_t is int");
_Static_assert(sizeof(long double) == 8 && _Alignof(long double) == 8, "long double is double");
_Static_assert(__BIGGEST_ALIGNMENT__ == 8, "biggest alignment is 8");
_Static_assert(__GCC_CONSTRUCTIVE_SIZE == 64 && __GCC_DESTRUCTIVE_SIZE == 64, "64-byte cache blocks");
_Static_assert(__GCC_ATOMIC_LLONG_LOCK_FREE == 2 && __GCC_ATOMIC_POINTER_LOCK_FREE == 2, "lock free 64-bit atomics");
#if !defined(__BLOCKS__) || !defined(__APPLE_CC__) || !defined(__APPLE__) || defined(__CHAR_UNSIGNED__) || defined(__WINT_UNSIGNED__)
#error "darwin predefines"
#endif
#ifdef __OBJC__
_Static_assert(__OBJC_BOOL_IS_BOOL == 1, "objc BOOL is bool");
#endif
#ifdef __cplusplus
extern "C++" { void takes_wint(__WINT_TYPE__) {} }
#endif
int abi_types_ok;
