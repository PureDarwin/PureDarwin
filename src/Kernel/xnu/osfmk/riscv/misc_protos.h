/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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
#ifndef _RISCV_MISC_PROTOS_H_
#define _RISCV_MISC_PROTOS_H_

#include <kern/kern_types.h>

typedef struct boot_args boot_args;
/* The address of the end of the kernelcache. */
extern vm_offset_t end_kern;
/* The lowest address in the kernelcache. */
extern vm_offset_t segLOWEST;

extern void machine_startup(__unused boot_args *args) __attribute__((noinline));

extern void riscv_auxkc_init(void *mh, void *base);

extern void riscv_vm_init(uint64_t memory_size, boot_args *args);
extern void riscv_vm_prot_init(boot_args *args);
extern void riscv_vm_prot_finalize(boot_args *args);

void __attribute__((__noreturn__)) _was_in_userspace(void);

extern kern_return_t DebuggerXCallEnter(boolean_t, bool);
extern void DebuggerXCallReturn(void);

extern void Load_context(thread_t);
extern void Idle_load_context(void) __attribute__((noreturn));
extern thread_t Switch_context(thread_t, thread_continue_t, thread_t);
extern thread_t Shutdown_context(void (*doshutdown)(processor_t), processor_t  processor);
extern void __dead2 Call_continuation(thread_continue_t, void *, wait_result_t, boolean_t enable_interrupts);

extern void DebuggerCall(unsigned int reason, void *ctx);
extern void DebuggerXCall(void *ctx);

extern int copyout_kern(const char *kernel_addr, user_addr_t user_addr, vm_size_t nbytes);
extern int copyin_kern(const user_addr_t user_addr, char *kernel_addr, vm_size_t nbytes);

extern void bcopy_phys(addr64_t from, addr64_t to, vm_size_t nbytes);
extern void bcopy_phys_with_options(addr64_t from, addr64_t to, vm_size_t nbytes, int options);

extern void dcache_incoherent_io_flush64(addr64_t pa, unsigned int count, unsigned int remaining, unsigned int *res);
extern void dcache_incoherent_io_store64(addr64_t pa, unsigned int count, unsigned int remaining, unsigned int *res);

extern int copyio_check_user_addr(user_addr_t user_addr, vm_size_t nbytes);

extern int apply_func_phys(addr64_t src64, vm_size_t bytes, int (*func)(void * buffer, vm_size_t bytes, void * arg), void * arg);

#endif /* _RISCV_MISC_PROTOS_H_ */
