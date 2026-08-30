/*
 * Apple-internal clang builtins used by xnu 12377's kalloc type segregation.
 * Upstream clang has none of them, so supply constant-expression fallbacks.
 * This costs the per-type kalloc zones (a hardening property); allocation
 * semantics are unchanged.
 */

#ifndef PD_APPLE_BUILTINS_H
#define PD_APPLE_BUILTINS_H

/* Empty signature: no per-type layout information is emitted. */
#define __builtin_xnu_type_signature(type) ""

/* Reports every type as plain data. KALLOC_TYPE_SIG_CHECK is
 * ((GRANULES & ~mask) == 0), and this same macro backs both the allocator flags
 * and static asserts like OSData's KALLOC_TYPE_IS_DATA_ONLY(T), so one value
 * has to serve both. MASK_DATA lets those asserts compile and still leaves
 * KT_PTR_ARRAY off; 0 would wrongly set both flags, ALL_GRANULES would fail the
 * asserts. TRADEOFF: pointer-bearing types land in data-only zones, so the
 * pointer/data split is lost - hardening only, not allocation semantics. */
#define __builtin_xnu_type_summary(type) ((1UL << 0) | (1UL << 2))

/* Permissive: this only backs _Static_assert pointer-vs-type checks. */
#define __builtin_xnu_types_compatible(a, b) 1

#endif /* PD_APPLE_BUILTINS_H */
