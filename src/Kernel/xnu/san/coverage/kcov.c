/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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
#include <string.h>
#include <stdbool.h>
#include <sys/sysctl.h>
#include <kern/cpu_number.h>
#include <kern/cpu_data.h>
#include <libkern/libkern.h>
#include <os/atomic_private.h>
#include <vm/pmap.h>
#include <machine/machine_routines.h>

#include <san/kcov.h>
#include <san/kcov_data.h>

#include <san/kcov_stksz.h>
#include <san/kcov_stksz_data.h>

#include <san/kcov_ksancov.h>
#include <san/kcov_ksancov_data.h>

/* Global flag that enables the sanitizer hook. */
static _Atomic unsigned int kcov_enabled = 0;


/*
 * Sysctl interface to coverage sanitizer.
 */
SYSCTL_DECL(_kern_kcov);
SYSCTL_NODE(_kern, OID_AUTO, kcov, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "kcov");
SYSCTL_COMPAT_INT(_kern_kcov, OID_AUTO, available, CTLFLAG_RD, NULL, 1, "");

/*
 * kern.kcov.sancov_compiled is 1 when XNU itself is compiled with SanitizerCoverage
 */
#if __has_feature(coverage_sanitizer)
SYSCTL_COMPAT_INT(_kern_kcov, OID_AUTO, sancov_compiled, CTLFLAG_RD, NULL, 1, "");
#else
SYSCTL_COMPAT_INT(_kern_kcov, OID_AUTO, sancov_compiled, CTLFLAG_RD, NULL, 0, "");
#endif

/*
 * Coverage sanitizer bootstrap.
 *
 * A compiler will add hooks almost in any basic block in the kernel. However it is
 * not safe to call hook from some of the contexts. To make this safe it would require
 * precise denylist of all unsafe sources. Which results in high maintenance costs.
 *
 * To avoid this we bootsrap the coverage sanitizer in phases:
 *
 *   1. Kernel starts with globaly disabled coverage sanitizer. At this point the hook
 *      can access safely only global variables.
 *   2. The boot cpu has allocated/configured per-cpu data. At this point the hook can
 *      use per-cpu data by using current_* but only on the boot cpu.
 *
 *   ... From this point we can start recording on boot cpu
 *
 *   3. Additional CPUs are added by kext. We rely on the fact that default value of
 *      per-cpu variable is 0. The assumption here is that some other (already configured)
 *      cpu is running the bootsrap of secondary CPU which is safe. Once secondary gets
 *      configured the boostrap originator enables its converage sanitizer by writing
 *      secondary's per-cpu data.
 *
 *      To make this step safe, it is required to maintain denylist that contains CPU
 *      bootstrap code to avoid firing hook from unsupported context.
 *
 *   ... From this point all CPUs can execute the hook correctly.
 *
 * This allows stack size monitoring during early boot. For all other cases we simply
 * boot with global set to 0 waiting for a client to actually enable sanitizer.
 */

/*
 * 1. & 2. enabling step. Must be called *after* per-cpu data are set up.
 */
__startup_func
static void
kcov_init(void)
{
	/* Master CPU is fully setup at this point so just enable coverage tracking. */
	ksancov_init();
	current_kcov_data()->kcd_enabled = 1;
}
STARTUP(EARLY_BOOT, STARTUP_RANK_LAST, kcov_init);

/*
 * 3. secondary CPU. Called on bootstrap originator after secondary is ready.
 */
void
kcov_start_cpu(int cpuid)
{
	/* No need to use atomics as we don't need to be so precise here. */
	cpu_kcov_data(cpuid)->kcd_enabled = 1;
}

void
kcov_enable(void)
{
	os_atomic_add(&kcov_enabled, 1, relaxed);
}

void
kcov_disable(void)
{
	os_atomic_sub(&kcov_enabled, 1, relaxed);
}


/*
 * Disable coverage sanitizer recording for given thread.
 */
static void
kcov_disable_thread(kcov_thread_data_t *data)
{
	data->ktd_disabled++;
}


/*
 * Enable coverage sanitizer recording for given thread.
 */
static void
kcov_enable_thread(kcov_thread_data_t *data)
{
	data->ktd_disabled--;
}


/*
 * Called when system enters panic code path with no return. There is no point in tracking
 * stack usage and delay (and possibly break) the coredump code.
 */
void
kcov_panic_disable(void)
{
	printf("KCOV: Disabling coverage tracking. System panicking.\n");
	/* Force disable the sanitizer hook. */
	os_atomic_store(&kcov_enabled, 0, relaxed);
}


/* Initialize per-thread sanitizer data for each new kernel thread. */
void
kcov_init_thread(kcov_thread_data_t *data)
{
	data->ktd_disabled = 0;

	kcov_ksancov_init_thread(&data->ktd_device);
	kcov_stksz_init_thread(&data->ktd_stksz);
}

/* Shared prologue between trace functions */
static kcov_thread_data_t *
trace_prologue(void)
{
	/* Check the global flag for the case no recording is enabled. */
	if (__probable(os_atomic_load(&kcov_enabled, relaxed) == 0)) {
		return NULL;
	}

	/*
	 * rdar://145659776
	 * If PAN is disabled we cannot safely re-enable preemption after disabling it.
	 * The proper way to do this in a generic way is to check here for PAN and bail ot
	 * if (__improbable(__builtin_arm_rsr("pan") == 0))
	 *
	 * The issue with this solution is the performance cost of reading the MSR for each
	 * trace point, so PAN disabled functions are included in the baclklist instead
	 * (see kcov-blacklist-arm64).
	 */

	/* Per-cpu area access. Must happen with disabled interrupts/preemtion. */
	disable_preemption();

	if (!current_kcov_data()->kcd_enabled) {
		enable_preemption();
		return NULL;
	}

	/* No support for PPL. */
	if (pmap_in_ppl()) {
		enable_preemption();
		return NULL;
	}
	/* Interrupt context not supported. */
	if (ml_at_interrupt_context()) {
		enable_preemption();
		return NULL;
	}

	thread_t th = current_thread();
	if (__improbable(th == THREAD_NULL)) {
		enable_preemption();
		return NULL;
	}

	/* This thread does not want to be traced. */
	kcov_thread_data_t *data = kcov_get_thread_data(th);
	if (__improbable(data->ktd_disabled) != 0) {
		enable_preemption();
		return NULL;
	}

	/* Enable preemption as we are no longer accessing per-cpu data. */
	enable_preemption();

	return data;
}

/*
 * This is the core of the coverage recording.
 *
 * A compiler inlines this function into every place eligible for instrumentation.
 * Every modification is very risky as added code may be called from unexpected
 * contexts (for example per-cpu data access).
 *
 * Do not call anything unnecessary before ksancov_disable() as that will cause
 * recursion. Update denylist after any such change.
 *
 * Every complex code here may have impact on the overall performance. This function
 * is called for every edge in the kernel and that means multiple times through a
 * single function execution.
 */
static void
trace_pc_guard(uint32_t __unused *guardp, void __unused *caller, uintptr_t __unused sp)
{
	kcov_ksancov_trace_guard(guardp, caller);

	kcov_thread_data_t *data = trace_prologue();
	if (data == NULL) {
		return;
	}

	/* It is now safe to call back to kernel from this thread without recursing in the hook itself. */
	kcov_disable_thread(data);

	kcov_stksz_update_stack_size(th, data, caller, sp);
	kcov_ksancov_trace_pc(data, guardp, caller, sp);

	kcov_enable_thread(data);
}

/*
 * Coverage Sanitizer ABI implementation.
 */


void
__sanitizer_cov_trace_pc_indirect(void * __unused callee)
{
	/* No indirect call recording support at this moment. */
	return;
}


__attribute__((nodebug))
void
__sanitizer_cov_trace_pc(void)
{
	uintptr_t sp = (uintptr_t)&sp;
	trace_pc_guard(NULL, __builtin_return_address(0), sp);
}


__attribute__((nodebug))
void
__sanitizer_cov_trace_pc_guard(uint32_t __unused *guardp)
{
	uintptr_t sp = (uintptr_t)&sp;
	trace_pc_guard(guardp, __builtin_return_address(0), sp);
}


void
__sanitizer_cov_trace_pc_guard_init(uint32_t __unused *start, uint32_t __unused *stop)
{
	kcov_ksancov_trace_pc_guard_init(start, stop);
}


void
__sanitizer_cov_pcs_init(uintptr_t __unused *start, uintptr_t __unused *stop)
{
	kcov_ksancov_pcs_init(start, stop);
}

static void
trace_cmp(uint32_t __unused type, uint64_t __unused arg1, uint64_t __unused arg2, void __unused *caller)
{
	kcov_thread_data_t *data = trace_prologue();
	if (data == NULL) {
		return;
	}

	/* It is now safe to call back to kernel from this thread without recursing in the hook itself. */
	kcov_disable_thread(data);

	kcov_ksancov_trace_cmp(data, type, arg1, arg2, caller);

	kcov_enable_thread(data);
}

void
__sanitizer_cov_trace_cmp1(uint8_t arg1, uint8_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE1, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_cmp2(uint16_t arg1, uint16_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE2, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_cmp4(uint32_t arg1, uint32_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE4, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_cmp8(uint64_t arg1, uint64_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE8, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp1(uint8_t arg1, uint8_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE1 | KCOV_CMP_CONST, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp2(uint16_t arg1, uint16_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE2 | KCOV_CMP_CONST, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp4(uint32_t arg1, uint32_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE4 | KCOV_CMP_CONST, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp8(uint64_t arg1, uint64_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE8 | KCOV_CMP_CONST, arg1, arg2, __builtin_return_address(0));
}

void
__sanitizer_cov_trace_switch(uint64_t val, uint64_t *cases)
{
	void *ret = __builtin_return_address(0);

	uint32_t type;
	switch (cases[1]) {
	case 8:
		type = KCOV_CMP_SIZE1 | KCOV_CMP_CONST;
		break;
	case 16:
		type = KCOV_CMP_SIZE2 | KCOV_CMP_CONST;
		break;
	case 32:
		type = KCOV_CMP_SIZE4 | KCOV_CMP_CONST;
		break;
	case 64:
		type = KCOV_CMP_SIZE8 | KCOV_CMP_CONST;
		break;
	default:
		return;
	}

	uint64_t i;
	uint64_t count = cases[0];

	for (i = 0; i < count; i++) {
		trace_cmp(type, cases[i + 2], val, ret);
	}
}

void
kcov_trace_cmp_func(void *caller_pc, uint32_t type, const void *s1, size_t s1len, const void *s2, size_t s2len, bool always_log)
{
	kcov_thread_data_t *data = trace_prologue();
	if (data == NULL) {
		return;
	}

	/* It is now safe to call back to kernel from this thread without recursing in the hook itself. */
	kcov_disable_thread(data);

	kcov_ksancov_trace_cmp_func(data, type, s1, s1len, s2, s2len, caller_pc, always_log);

	kcov_enable_thread(data);
}
