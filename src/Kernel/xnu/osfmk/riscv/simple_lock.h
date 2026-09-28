/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#ifdef  KERNEL_PRIVATE

#ifndef _RISCV_SIMPLE_LOCK_TYPES_H_
#define _RISCV_SIMPLE_LOCK_TYPES_H_

#include <kern/lock_group.h>

#ifdef  KERNEL_PRIVATE
#include <mach/boolean.h>
#include <kern/lock_types.h>
#include <sys/appleapiopts.h>
#ifdef  MACH_KERNEL_PRIVATE
#include <riscv/locks.h>
#include <mach_ldebug.h>
#endif

#ifdef MACH_KERNEL_PRIVATE

extern machine_timeout_t LockTimeOut;     /* Number of hardware ticks of a lock timeout */
extern machine_timeout_t LockTimeOutUsec; /* Number of microseconds for lock timeout */

typedef lck_spin_t usimple_lock_data_t, *usimple_lock_t;
#else /* MACH_KERNEL_PRIVATE */

typedef struct slock {
	uint64_t lock_data[9];
} usimple_lock_data_t, *usimple_lock_t;

#endif  /* MACH_KERNEL_PRIVATE */

#define USIMPLE_LOCK_NULL       ((usimple_lock_t) 0)

#if !defined(decl_simple_lock_data)

typedef usimple_lock_data_t     *simple_lock_t;
typedef usimple_lock_data_t     simple_lock_data_t;

#define decl_simple_lock_data(class, name) \
	class	simple_lock_data_t	name

#endif  /* !defined(decl_simple_lock_data) */

#ifdef  MACH_KERNEL_PRIVATE

#define MACHINE_SIMPLE_LOCK

extern void     riscv_usimple_lock_init(simple_lock_t, __unused unsigned short);

#define simple_lock_init(l, t)   riscv_usimple_lock_init(l,t)

#if LCK_GRP_USE_ARG
#define simple_lock(l, grp)                  lck_spin_lock_grp(l, grp)
#define simple_lock_nopreempt(l, grp)        lck_spin_lock_nopreempt_grp(l, grp)
#define simple_lock_try(l, grp)              lck_spin_try_lock_grp(l, grp)
#define simple_lock_try_nopreempt(l, grp)    lck_spin_try_lock_nopreempt_grp(l, grp)
#else
#define simple_lock(l, grp)                  lck_spin_lock(l)
#define simple_lock_nopreempt(l, grp)        lck_spin_lock_nopreempt(l)
#define simple_lock_try(l, grp)              lck_spin_try_lock(l)
#define simple_lock_try_nopreempt(l, grp)    lck_spin_try_lock_nopreempt(l)
#endif /* LCK_GRP_USE_ARG */

#define simple_unlock(l)                lck_spin_unlock(l)
#define simple_unlock_nopreempt(l)      lck_spin_unlock_nopreempt(l)

#define simple_lock_try_lock_loop(l, grp)    simple_lock(l, grp)

#define simple_lock_addr(l)             (&(l))
#define simple_lock_assert(l, t) lck_spin_assert(l,t)
#define kdp_simple_lock_is_acquired(l) kdp_lck_spin_is_acquired(l)

#endif  /* MACH_KERNEL_PRIVATE */
#endif  /* KERNEL_PRIVATE */

#endif /* !_RISCV_SIMPLE_LOCK_TYPES_H_ */

#endif  /* KERNEL_PRIVATE */
