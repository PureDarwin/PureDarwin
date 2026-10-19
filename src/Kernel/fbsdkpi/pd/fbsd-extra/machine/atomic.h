// SPDX-License-Identifier: MIT
// FreeBSD's machine/atomic.h for the linuxkpi layer
#pragma once
// FreeBSD atomic(9) on compiler builtins, set and clear are bit operations

#define mb() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
#define rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_thread_fence_acq() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_thread_fence_rel() __atomic_thread_fence(__ATOMIC_RELEASE)
#define atomic_thread_fence_acq_rel() __atomic_thread_fence(__ATOMIC_ACQ_REL)
#define atomic_thread_fence_seq_cst() __atomic_thread_fence(__ATOMIC_SEQ_CST)

#define PD_ATOMIC_OPS(name, type) \
static __inline void atomic_add_##name(volatile type *p, type v) { __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); } \
static __inline void atomic_subtract_##name(volatile type *p, type v) { __atomic_fetch_sub(p, v, __ATOMIC_SEQ_CST); } \
static __inline void atomic_set_##name(volatile type *p, type v) { __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST); } \
static __inline void atomic_clear_##name(volatile type *p, type v) { __atomic_fetch_and(p, ~v, __ATOMIC_SEQ_CST); } \
static __inline type atomic_load_##name(volatile type *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); } \
static __inline type atomic_load_acq_##name(volatile type *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); } \
static __inline void atomic_store_##name(volatile type *p, type v) { __atomic_store_n(p, v, __ATOMIC_RELAXED); } \
static __inline void atomic_store_rel_##name(volatile type *p, type v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); } \
static __inline type atomic_fetchadd_##name(volatile type *p, type v) { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); } \
static __inline type atomic_swap_##name(volatile type *p, type v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); } \
static __inline type atomic_readandclear_##name(volatile type *p) { return __atomic_exchange_n(p, 0, __ATOMIC_SEQ_CST); } \
static __inline int atomic_cmpset_##name(volatile type *p, type o, type n) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); } \
static __inline int atomic_fcmpset_##name(volatile type *p, type *o, type n) { return __atomic_compare_exchange_n(p, o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); } \
static __inline int atomic_testandset_##name(volatile type *p, unsigned b) { type m = (type)1 << (b % (sizeof(type) * 8)); return (__atomic_fetch_or(p, m, __ATOMIC_SEQ_CST) & m) != 0; } \
static __inline int atomic_testandclear_##name(volatile type *p, unsigned b) { type m = (type)1 << (b % (sizeof(type) * 8)); return (__atomic_fetch_and(p, ~m, __ATOMIC_SEQ_CST) & m) != 0; }

PD_ATOMIC_OPS(char, unsigned char)
PD_ATOMIC_OPS(short, unsigned short)
PD_ATOMIC_OPS(int, unsigned int)
PD_ATOMIC_OPS(long, unsigned long)
PD_ATOMIC_OPS(8, unsigned char)
PD_ATOMIC_OPS(16, unsigned short)
PD_ATOMIC_OPS(32, unsigned int)
PD_ATOMIC_OPS(64, unsigned long)
PD_ATOMIC_OPS(ptr, __UINTPTR_TYPE__)

#define atomic_add_acq_int atomic_add_int
#define atomic_add_rel_int atomic_add_int
#define atomic_subtract_rel_int atomic_subtract_int
#define atomic_set_acq_int atomic_set_int
#define atomic_cmpset_acq_int atomic_cmpset_int
#define atomic_cmpset_rel_int atomic_cmpset_int
#define atomic_fcmpset_acq_int atomic_fcmpset_int
#define atomic_fcmpset_rel_int atomic_fcmpset_int
#define atomic_cmpset_acq_ptr atomic_cmpset_ptr
#define atomic_cmpset_rel_ptr atomic_cmpset_ptr
#define atomic_load_acq_ptr atomic_load_acq_ptr
