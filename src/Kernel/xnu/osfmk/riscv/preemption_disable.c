/*
 * Copyright (c) 2007-2023 Apple Inc. All rights reserved.
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

// Routines for preemption disablement, which keeps the current thread on its current CPU.

#include <riscv/cpu_data.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/proc_reg.h>
#include <kern/cpu_data.h>
#include <kern/percpu.h>
#include <kern/thread.h>
#include <kern/timeout.h>
#include <mach/machine/sdt.h>
#include <os/base.h>
#include <stdint.h>
#include <sys/kdebug.h>

#if SCHED_HYGIENE_DEBUG
static void
_do_disable_preemption_without_measurements(void);
#endif

// Checks for a pended AST_URGENT after preemption is reenabled. Racing a CPU change is benign since a
// context switch clears AST_URGENT, so there are no false negatives and interrupts stay untouched.
static OS_NOINLINE
void
kernel_preempt_check(void)
{
	uint64_t state;

	/* If interrupts are masked, we can't take an AST here */
	state = csr_read(sstatus);
	if (!(state & SSTATUS_SIE)) {
		return;
	}

	/* disable interrupts */
	csr_clear(sstatus, SSTATUS_SIE);

	// Reload cpu_pending_ast now that interrupts are disabled, to debounce false positives.
	if (current_thread()->machine.CpuDatap->cpu_pending_ast & AST_URGENT) {
		ast_taken_kernel();
	}

	/* restore the original interrupt mask */
	csr_set(sstatus, SSTATUS_SIE);
}

static inline void
_enable_preemption_write_count(thread_t thread, unsigned int count)
{
	os_atomic_store(&thread->machine.preemption_count, count, compiler_acq_rel);

	// Racy, may read another CPU's pending_ast mask, but can't have false negatives.
	if (count == 0) {
		if (__improbable(thread->machine.CpuDatap->cpu_pending_ast & AST_URGENT)) {
			return kernel_preempt_check();
		}
	}
}

// Kept tiny so it inlines, most callers already have current_thread().
// /!\ Breaking inlining makes zalloc ~10% slower /!\
OS_ALWAYS_INLINE __mockable
void
_disable_preemption(void)
{
	thread_t thread = current_thread();
	unsigned int count = thread->machine.preemption_count;

	os_atomic_store(&thread->machine.preemption_count,
	    count + 1, compiler_acq_rel);

#if SCHED_HYGIENE_DEBUG
	// ISR and PPL entry/exit also modify the count, but they pair increment/decrement
	// so collection here stays in sync.
	if (improbable_static_if(sched_debug_preemption_disable)) {
		if (__improbable(count == 0)) {
			__attribute__((musttail))
			return _prepare_preemption_disable_measurement();
		}
	}
#endif /* SCHED_HYGIENE_DEBUG */
}

// Variant of disable_preemption() that takes no measurements.
OS_ALWAYS_INLINE __mockable
void
_disable_preemption_without_measurements(void)
{
	thread_t thread = current_thread();
	unsigned int count = thread->machine.preemption_count;

#if SCHED_HYGIENE_DEBUG
	_do_disable_preemption_without_measurements();
#endif /* SCHED_HYGIENE_DEBUG */

	os_atomic_store(&thread->machine.preemption_count,
	    count + 1, compiler_acq_rel);
}

// Non-inlineable panic helpers so _enable_preemption() inlines everywhere with LTO.
__abortlike
static void
_enable_preemption_underflow(void)
{
	panic("Preemption count underflow");
}

// Kept tiny so it inlines. SCHED_HYGIENE_MARKER avoids loading unrelated current_thread() fields.
// /!\ Breaking inlining makes zalloc ~10% slower /!\
OS_ALWAYS_INLINE __mockable
void
_enable_preemption(void)
{
	thread_t thread = current_thread();
	unsigned int count  = thread->machine.preemption_count;

	if (__improbable(count == 0)) {
		_enable_preemption_underflow();
	}

#if SCHED_HYGIENE_DEBUG
	if (improbable_static_if(sched_debug_preemption_disable)) {
		if (__improbable(count == SCHED_HYGIENE_MARKER + 1)) {
			return _collect_preemption_disable_measurement();
		}
	}
#endif /* SCHED_HYGIENE_DEBUG */

	_enable_preemption_write_count(thread, count - 1);
}

OS_ALWAYS_INLINE
unsigned int
get_preemption_level_for_thread(thread_t thread)
{
	unsigned int count = thread->machine.preemption_count;

#if SCHED_HYGIENE_DEBUG
	// hide this "flag" from callers, it would also make the count look negative
	count &= ~SCHED_HYGIENE_MARKER;
#endif
	return (int)count;
}

OS_ALWAYS_INLINE
int
get_preemption_level(void)
{
	return get_preemption_level_for_thread(current_thread());
}

#if SCHED_HYGIENE_DEBUG

// the measurement window of each cpu, arm keeps this in a header for its ppl
struct _preemption_disable_pcpu {
	kern_timeout_t pdp_timeout;
	_Atomic uint64_t pdp_max_mach_duration;
};

uint64_t _Atomic PERCPU_DATA_HACK_78750602(preemption_disable_max_mt);

#if XNU_PLATFORM_iPhoneOS
#define DEFAULT_PREEMPTION_TIMEOUT 120000 /* 5ms */
#define DEFAULT_PREEMPTION_MODE SCHED_HYGIENE_MODE_PANIC
#elif XNU_PLATFORM_XROS
#define DEFAULT_PREEMPTION_TIMEOUT 24000  /* 1ms */
#define DEFAULT_PREEMPTION_MODE SCHED_HYGIENE_MODE_PANIC
#else
#define DEFAULT_PREEMPTION_TIMEOUT 0      /* Disabled */
#define DEFAULT_PREEMPTION_MODE SCHED_HYGIENE_MODE_OFF
#endif /* XNU_PLATFORM_iPhoneOS */

MACHINE_TIMEOUT_DEV_WRITEABLE(sched_preemption_disable_threshold_mt, "sched-preemption",
    DEFAULT_PREEMPTION_TIMEOUT, MACHINE_TIMEOUT_UNIT_TIMEBASE, kprintf_spam_mt_pred);
TUNABLE_DT_WRITEABLE(sched_hygiene_mode_t, sched_preemption_disable_debug_mode,
    "machine-timeouts",
    "sched-preemption-disable-mode", /* DT property names have to be 31 chars max */
    "sched_preemption_disable_debug_mode",
    DEFAULT_PREEMPTION_MODE,
    TUNABLE_DT_CHECK_CHOSEN);

struct _preemption_disable_pcpu PERCPU_DATA(_preemption_disable_pcpu_data);

// Start a preemption disable timeout window for this CPU. Interrupts must be disabled (assert elided).
OS_ALWAYS_INLINE
static void
_preemption_disable_snap_start(void)
{
	struct _preemption_disable_pcpu *pcpu = PERCPU_GET(_preemption_disable_pcpu_data);
	const timeout_flags_t flags = ML_TIMEOUT_TIMEBASE_FLAGS | ML_TIMEOUT_PMC_FLAGS | TF_SAMPLE_INTERRUPT_TIME | TF_BACKTRACE;

	kern_timeout_start(&pcpu->pdp_timeout, flags);
}

// End the window from _preemption_disable_snap_start(), returning current times in top (PMCs not read).
// Returns whether to abandon the measurement due to abandon_preemption_disable_measurement().
OS_ALWAYS_INLINE
static bool
_preemption_disable_snap_end(kern_timeout_t *top)
{
	struct _preemption_disable_pcpu *pcpu = PERCPU_GET(_preemption_disable_pcpu_data);
	const timeout_flags_t flags = ML_TIMEOUT_TIMEBASE_FLAGS | TF_SAMPLE_INTERRUPT_TIME;
	const bool int_masked_debug = false;
	const bool istate = ml_set_interrupts_enabled_with_debug(false, int_masked_debug);
	// Grab start and current time with interrupts off, or an interrupt adjusting preemption_disable_mt
	// could inflate the measurement (or make it negative).
	kern_timeout_end(&pcpu->pdp_timeout, flags);

	const uint64_t max_duration = os_atomic_load(&pcpu->pdp_max_mach_duration, relaxed);
	const uint64_t gross_duration = kern_timeout_gross_duration(&pcpu->pdp_timeout);
	if (__improbable(gross_duration > max_duration)) {
		os_atomic_store(&pcpu->pdp_max_mach_duration, gross_duration, relaxed);
	}

	*top = pcpu->pdp_timeout;
	ml_set_interrupts_enabled_with_debug(istate, int_masked_debug);

	return gross_duration == 0;
}

OS_NOINLINE
void
_prepare_preemption_disable_measurement(void)
{
	thread_t thread = current_thread();

	if (thread->machine.int_handler_addr == 0 &&
	    sched_preemption_disable_debug_mode) {
		// Only measure outside interrupt handlers. We want net disabled time minus interrupt time,
		// and interrupt handling duration is tracked separately.

		bool const int_masked_debug = false;
		bool istate = ml_set_interrupts_enabled_with_debug(false, int_masked_debug);
		thread->machine.preemption_count |= SCHED_HYGIENE_MARKER;
		_preemption_disable_snap_start();
		ml_set_interrupts_enabled_with_debug(istate, int_masked_debug);
	}
}

OS_NOINLINE
void
_collect_preemption_disable_measurement(void)
{
	kern_timeout_t to;
	const bool abandon = _preemption_disable_snap_end(&to);

	if (__improbable(abandon)) {
		goto out;
	}

	const uint64_t gross_duration = kern_timeout_gross_duration(&to);
	const uint64_t threshold = os_atomic_load(&sched_preemption_disable_threshold_mt, relaxed);
	if (__improbable(threshold > 0 && gross_duration >= threshold)) {
		// Double check that the time spent not handling interrupts is over the threshold.
		const int64_t net_duration = kern_timeout_net_duration(&to);
		uint64_t average_cpi_whole, average_cpi_fractional;

		assert3u(net_duration, >=, 0);
		if (net_duration < threshold) {
			goto out;
		}

		if (__probable(sched_preemption_disable_debug_mode == SCHED_HYGIENE_MODE_PANIC)) {
			kern_timeout_try_panic(KERN_TIMEOUT_PREEMPTION, 0, &to,
			    "preemption disable timeout exceeded:", threshold);
		}

		kern_timeout_cpi(&to, &average_cpi_whole, &average_cpi_fractional);

		DTRACE_SCHED4(mach_preemption_expired, uint64_t, net_duration, uint64_t, gross_duration,
		    uint64_t, average_cpi_whole, uint64_t, average_cpi_fractional);
		KDBG(MACHDBG_CODE(DBG_MACH_SCHED, MACH_PREEMPTION_EXPIRED), net_duration, gross_duration, average_cpi_whole, average_cpi_fractional);
	}

out:
	// the preemption count is SCHED_HYGIENE_MARKER, clear it.
	_enable_preemption_write_count(current_thread(), 0);
}

// Abandon a potential measurement, e.g. for the idle thread, which would spuriously trip the threshold.
void
abandon_preemption_disable_measurement(void)
{
	struct _preemption_disable_pcpu *pcpu = PERCPU_GET(_preemption_disable_pcpu_data);

	kern_timeout_override(&pcpu->pdp_timeout);
}

/* Inner part of disable_preemption_without_measuerments() */
OS_ALWAYS_INLINE
static void
_do_disable_preemption_without_measurements(void)
{
	// Tell _collect_preemption_disable_measurement() we didn't really care.
	struct _preemption_disable_pcpu *pcpu = PERCPU_GET(_preemption_disable_pcpu_data);
	kern_timeout_override(&pcpu->pdp_timeout);
}

void preemption_disable_reset_max_durations(void);
void
preemption_disable_reset_max_durations(void)
{
	percpu_foreach(pcpu, _preemption_disable_pcpu_data) {
		os_atomic_store(&pcpu->pdp_max_mach_duration, 0, relaxed);
	}
}

unsigned int preemption_disable_get_max_durations(uint64_t *durations, size_t count);
unsigned int
preemption_disable_get_max_durations(uint64_t *durations, size_t count)
{
	int cpu = 0;
	percpu_foreach(pcpu, _preemption_disable_pcpu_data) {
		if (cpu < count) {
			durations[cpu++] = os_atomic_load(&pcpu->pdp_max_mach_duration, relaxed);
		}
	}
	return cpu;
}

// Skip predicate for sched_preemption_disable, which trips spuriously with kprintf spam.
bool
kprintf_spam_mt_pred(struct machine_timeout_spec const __unused *spec)
{
	bool const kprintf_spam_enabled = !(disable_kprintf_output || disable_serial_output);
	return kprintf_spam_enabled;
}

// Abandon function exported for AppleCLPC only, a workaround for rdar://91668370.
void
sched_perfcontrol_abandon_preemption_disable_measurement(void)
{
	abandon_preemption_disable_measurement();
}

#else /* SCHED_HYGIENE_DEBUG */

void
abandon_preemption_disable_measurement(void)
{
	// No-op. Function is exported, so needs to be defined
}

void
sched_perfcontrol_abandon_preemption_disable_measurement(void)
{
	// No-op. Function is exported, so needs to be defined
}

#endif /* SCHED_HYGIENE_DEBUG */
