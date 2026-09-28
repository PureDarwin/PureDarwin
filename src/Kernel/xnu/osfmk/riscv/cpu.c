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
// cpu specific routines.

#include <pexpert/riscv/board_config.h>
#include <kern/kalloc.h>
#include <kern/machine.h>
#include <kern/cpu_number.h>
#include <kern/percpu.h>
#include <kern/thread.h>
#include <kern/timer_queue.h>
#include <kern/monotonic.h>
#include <riscv/cpu_data.h>
#include <riscv/caches_internal.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/machine_cpu.h>
#include <riscv/rtclock.h>
#include <riscv/proc_reg.h>
#include <riscv/sbi.h>
#include <mach/processor_info.h>
#include <vm/pmap.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_map.h>
#include <pexpert/riscv/protos.h>
#include <pexpert/device_tree.h>
#include <sys/kdebug.h>
#include <riscv/machine_routines.h>
#include <machine/atomic.h>
#include <san/kasan.h>

#if KPERF
#include <kperf/kptimer.h>
#endif /* KPERF */

#include <libkern/section_keywords.h>

extern boolean_t        idle_enable;
extern uint64_t         wake_abstime;

// secondary harts enter here through sbi hart_start, see start.s
extern char             start_cpu[];

// cpu_sleep leaves this in a stopping hart's cpu_data just before the hart stops
#define RISCV_CPU_ON_SLEEP_PATH 0x50535553UL

// scounteren.tm lets user mode read the time csr
#define SCOUNTEREN_TM   (1ULL << 1)

// wfi mode: 0 disabled, 1 normal.
TUNABLE(unsigned int, wfi, "wfi", 1);

void
cpu_bootstrap(void)
{
}

void
cpu_sleep(void)
{
	cpu_data_t     *cpu_data_ptr = getCpuDatap();

	cpu_data_ptr->cpu_active_thread = current_thread();
	os_atomic_or(&cpu_data_ptr->cpu_flags, SleepState, relaxed);

#if KPERF
	kptimer_stop_curcpu();
#endif /* KPERF */

	platform_cache_clean();

	if (ml_is_quiescing()) {
		PE_cpu_machine_quiesce(cpu_data_ptr->cpu_id);
	} else {
		// cpu_start brings the hart back through start_cpu with a fresh hart_start
		cpu_data_ptr->cpu_sleep_token = RISCV_CPU_ON_SLEEP_PATH;
		__asm__ volatile ("fence rw, rw" ::: "memory");
		long err = sbi_hart_stop();
		panic("cpu_sleep: sbi hart_stop on cpu %d failed %ld", cpu_data_ptr->cpu_number, err);
	}
	/*NOTREACHED*/
}

// True if an enabled interrupt is pending on this hart.
bool
cpu_interrupt_is_pending(void)
{
	return (csr_read(sip) & csr_read(sie)) != 0;
}

static bool
cpu_proximate_timer(void)
{
	return !SetIdlePop();
}

void __attribute__((noreturn))
cpu_idle(void)
{
	cpu_data_t     *cpu_data_ptr = getCpuDatap();
	uint64_t        new_idle_timeout_ticks = 0x0ULL, lastPop;
	bool idle_disallowed = false;

	if (__improbable((!idle_enable))) {
		idle_disallowed = true;
	} else if (__improbable(cpu_data_ptr->cpu_signal & SIGPdisabled)) {
		idle_disallowed = true;
	}

	if (__improbable(idle_disallowed)) {
		Idle_load_context();
	}

	if (__improbable(cpu_proximate_timer())) {
		// wfi returns once the timer is pending even with sstatus.sie clear
		if (wfi) {
			__asm__ volatile ("wfi");
		}
		/* Back to machine_idle() */
		Idle_load_context();
	}

	lastPop = cpu_data_ptr->rtcPop;

	cpu_data_ptr->cpu_active_thread = current_thread();

	if (wfi && (cpu_data_ptr->cpu_idle_notify != NULL)) {
		cpu_data_ptr->cpu_idle_notify(cpu_data_ptr->cpu_id, TRUE, &new_idle_timeout_ticks);
	}

	if (cpu_data_ptr->idle_timer_notify != NULL) {
		if (new_idle_timeout_ticks == 0x0ULL) {
			/* turn off the idle timer */
			cpu_data_ptr->idle_timer_deadline = 0x0ULL;
		} else {
			/* set the new idle timeout */
			clock_absolutetime_interval_to_deadline(new_idle_timeout_ticks, &cpu_data_ptr->idle_timer_deadline);
		}
		timer_resync_deadlines();
		if (cpu_data_ptr->rtcPop != lastPop) {
			SetIdlePop();
		}
	}

	if (wfi) {
		platform_cache_idle_enter();

		__asm__ volatile ("fence iorw, iorw" ::: "memory");
		__asm__ volatile ("wfi");
	}

	ClearIdlePop(TRUE);

	cpu_idle_exit(FALSE);
}

void
cpu_idle_exit(__unused boolean_t from_reset)
{
	uint64_t        new_idle_timeout_ticks = 0x0ULL;
	cpu_data_t     *cpu_data_ptr = getCpuDatap();

	if (wfi && (cpu_data_ptr->cpu_idle_notify != NULL)) {
		cpu_data_ptr->cpu_idle_notify(cpu_data_ptr->cpu_id, FALSE, &new_idle_timeout_ticks);
	}

	if (cpu_data_ptr->idle_timer_notify != NULL) {
		if (new_idle_timeout_ticks == 0x0ULL) {
			/* turn off the idle timer */
			cpu_data_ptr->idle_timer_deadline = 0x0ULL;
		} else {
			/* set the new idle timeout */
			clock_absolutetime_interval_to_deadline(new_idle_timeout_ticks, &cpu_data_ptr->idle_timer_deadline);
		}
		timer_resync_deadlines();
	}

	Idle_load_context();
}

void
cpu_init(void)
{
	cpu_data_t     *cdp = getCpuDatap();

	if (cdp->cpu_type != CPU_TYPE_RISCV64) {
		cdp->cpu_type = CPU_TYPE_RISCV64;

		timer_call_queue_init(&cdp->rtclock_timer.queue);
		cdp->rtclock_timer.deadline = EndOfAllTime;

		if (cdp != &BootCpuData) {
			// Non-boot CPUs are initialized here, the boot CPU in pmap_bootstrap.
			pmap_cpu_data_init();
		}

		cdp->cpu_subtype = CPU_SUBTYPE_RISCV_ALL;
		cdp->cpu_threadtype = CPU_THREADTYPE_NONE;
	}
	cdp->cpu_stat.irq_ex_cnt_wake = 0;
	cdp->cpu_stat.ipi_cnt_wake = 0;
	cdp->cpu_running = TRUE;
	cdp->cpu_sleep_token_last = cdp->cpu_sleep_token;
	cdp->cpu_sleep_token = 0x0UL;

	// ipis may arrive as soon as the hart takes interrupts, external ones wait for a handler
	csr_set(sie, SIE_SSIE);
	if (cdp->interrupt_handler != NULL) {
		csr_set(sie, SIE_SEIE);
	}
	csr_set(scounteren, SCOUNTEREN_TM);
}

void
cpu_stack_alloc(cpu_data_t *cpu_data_ptr)
{
	vm_offset_t irq_stack = 0;
	vm_offset_t exc_stack = 0;

	kmem_alloc(kernel_map, &irq_stack,
	    INTSTACK_SIZE + ptoa(2), KMA_NOFAIL | KMA_PERMANENT | KMA_ZERO |
	    KMA_GUARD_FIRST | KMA_GUARD_LAST | KMA_KSTACK | KMA_KOBJECT,
	    VM_KERN_MEMORY_STACK);

	cpu_data_ptr->intstack_top = irq_stack + PAGE_SIZE + INTSTACK_SIZE;
	cpu_data_ptr->istackptr = (void *)cpu_data_ptr->intstack_top;

	kmem_alloc(kernel_map, &exc_stack,
	    EXCEPSTACK_SIZE + ptoa(2), KMA_NOFAIL | KMA_PERMANENT | KMA_ZERO |
	    KMA_GUARD_FIRST | KMA_GUARD_LAST | KMA_KSTACK | KMA_KOBJECT,
	    VM_KERN_MEMORY_STACK);

	cpu_data_ptr->excepstack_top = exc_stack + PAGE_SIZE + EXCEPSTACK_SIZE;
	cpu_data_ptr->excepstackptr = (void *)cpu_data_ptr->excepstack_top;
}

void
cpu_data_init(cpu_data_t *cpu_data_ptr)
{
	os_atomic_store(&cpu_data_ptr->cpu_flags, 0, relaxed);
	cpu_data_ptr->cpu_int_state = 0;
	cpu_data_ptr->cpu_pending_ast = AST_NONE;
	cpu_data_ptr->cpu_cache_dispatch = NULL;
	cpu_data_ptr->rtcPop = EndOfAllTime;
	cpu_data_ptr->rtclock_datap = &RTClockData;

	cpu_data_ptr->cpu_base_timebase = 0;
	cpu_data_ptr->cpu_idle_notify = NULL;
	cpu_data_ptr->cpu_idle_latency = 0x0ULL;
	cpu_data_ptr->cpu_idle_pop = 0x0ULL;
	cpu_data_ptr->cpu_phys_id = 0x0UL;
	cpu_data_ptr->cpu_cluster_type = CLUSTER_TYPE_SMP;
	cpu_data_ptr->cpu_cluster_id = 0;
	cpu_data_ptr->cpu_l2_id = 0;
	cpu_data_ptr->cpu_l2_size = 0;
	cpu_data_ptr->cpu_l3_id = 0;
	cpu_data_ptr->cpu_l3_size = 0;

	cpu_data_ptr->cpu_signal = SIGPdisabled;

	cpu_data_ptr->cpu_get_decrementer_func = NULL;
	cpu_data_ptr->cpu_set_decrementer_func = NULL;
	cpu_data_ptr->cpu_sleep_token = RISCV_CPU_ON_SLEEP_PATH;
	cpu_data_ptr->cpu_sleep_token_last = 0x00000000UL;
	cpu_data_ptr->cpu_xcall_p0 = NULL;
	cpu_data_ptr->cpu_xcall_p1 = NULL;
	cpu_data_ptr->cpu_imm_xcall_p0 = NULL;
	cpu_data_ptr->cpu_imm_xcall_p1 = NULL;

	pmap_cpu_data_t * pmap_cpu_data_ptr = &cpu_data_ptr->cpu_pmap_cpu_data;

	pmap_cpu_data_ptr->cpu_number = PMAP_INVALID_CPU_NUM;
	pmap_cpu_data_ptr->cpu_nested_pmap = (struct pmap *) NULL;
	pmap_cpu_data_ptr->cpu_user_pmap = (struct pmap *) NULL;

	cpu_data_ptr->halt_status = CPU_NOT_HALTED;
}

void
cpu_data_register(cpu_data_t *cpu_data_ptr)
{
	int     cpu = cpu_data_ptr->cpu_number;

#if KASAN
	for (int i = 0; i < CPUWINDOWS_MAX; i++) {
		kasan_notify_address_nopoison(pmap_cpu_windows_copy_addr(cpu, i), PAGE_SIZE);
	}
#endif

	__asm__ volatile ("fence rw, rw" ::: "memory"); // Ensure prior stores to cpu data are visible
	CpuDataEntries[cpu].cpu_data_vaddr = cpu_data_ptr;
	CpuDataEntries[cpu].cpu_data_paddr = (void *)ml_vtophys((vm_offset_t)cpu_data_ptr);
}

void
cpu_start(int cpu)
{
	cpu_data_t *cpu_data_ptr = CpuDataEntries[cpu].cpu_data_vaddr;
	processor_t processor = PERCPU_GET_RELATIVE(processor, cpu_data, cpu_data_ptr);

	if (processor_should_kprintf(processor, true)) {
		kprintf("cpu_start() cpu: %d\n", cpu);
	}

	if (cpu == cpu_number()) {
		/* Current CPU is already running, just needs initialization */
		cpu_machine_init();
	} else {
		thread_t first_thread;
		long err;

		cpu_data_ptr->cpu_pmap_cpu_data.cpu_nested_pmap = NULL;

		if (processor->startup_thread != THREAD_NULL) {
			first_thread = processor->startup_thread;
		} else {
			first_thread = processor->idle_thread;
		}
		cpu_data_ptr->cpu_active_thread = first_thread;
		first_thread->machine.CpuDatap = cpu_data_ptr;
		first_thread->machine.pcpu_data_base_and_cpu_number =
		    ml_make_pcpu_base_and_cpu_number((vm_address_t)cpu_data_ptr - __PERCPU_ADDR(cpu_data),
		    cpu_data_ptr->cpu_number);

		// the hart starts with paging off and reads its cpu_data by physical address
		flush_dcache((vm_offset_t)&CpuDataEntries[cpu], sizeof(cpu_data_entry_t), FALSE);
		flush_dcache((vm_offset_t)cpu_data_ptr, sizeof(cpu_data_t), FALSE);

		err = sbi_hart_start(cpu_data_ptr->cpu_phys_id,
		    kvtophys((vm_offset_t)start_cpu), kvtophys((vm_offset_t)cpu_data_ptr));
		if (err != SBI_SUCCESS) {
			panic("cpu_start: sbi hart_start of cpu %d (hart %u) failed %ld",
			    cpu, cpu_data_ptr->cpu_phys_id, err);
		}
	}
}


void
cpu_timebase_init(boolean_t from_boot)
{
	cpu_data_t *cdp = getCpuDatap();
	uint64_t timebase_offset = 0;

	if (cdp->cpu_get_decrementer_func == NULL) {
		cdp->cpu_get_decrementer_func = rtclock_timebase_func.tbd_get_decrementer;
		cdp->cpu_set_decrementer_func = rtclock_timebase_func.tbd_set_decrementer;
	}

	if (!from_boot && (cdp == &BootCpuData) && ml_is_quiescing()) {
		// The timebase may have kept ticking or reset across sleep. Offset the clock so it reads
		// wake_abstime when the boot CPU calls cpu_timebase_init, so mach_absolute_time() stops.
		rtclock_base_abstime = wake_abstime - ml_get_hwclock();
	} else if (from_boot) {
#if DEBUG || DEVELOPMENT
		if (PE_parse_boot_argn("timebase_offset", &timebase_offset, sizeof(timebase_offset))) {
			rtclock_base_abstime += timebase_offset;
		}
#endif
		/* On initial boot, initialize time_since_reset to the time csr. */
		ml_set_reset_time(ml_get_hwclock());
	}

	cdp->cpu_decrementer = 0x7FFFFFFFUL;
	cdp->cpu_timebase = timebase_offset;
	cdp->cpu_base_timebase = rtclock_base_abstime;
}

int
cpu_cluster_id(void)
{
	return getCpuDatap()->cpu_cluster_id;
}

void
cpu_machine_idle_init(boolean_t from_boot)
{
	if (from_boot) {
		unsigned long   jtag = 0;

		if (PE_parse_boot_argn("jtag", &jtag, sizeof(jtag))) {
			if (jtag != 0) {
				idle_enable = FALSE;
			} else {
				idle_enable = TRUE;
			}
		} else {
			idle_enable = TRUE;
		}

		// bits 7..0 give the wfi type
		switch (wfi & 0xff) {
		case 0:
			// disable wfi
			wfi = 0;
			break;

		case 1:
		default:
			// do nothing
			break;
		}
	}

	// a hart keeps its state across wfi, idle needs no resume vector
	clean_dcache((vm_offset_t)getCpuDatap(), sizeof(cpu_data_t), FALSE);
}

_Atomic uint32_t cpu_idle_count = 0;

void
machine_track_platform_idle(boolean_t entry)
{
	if (entry) {
		os_atomic_inc(&cpu_idle_count, relaxed);
	} else {
		os_atomic_dec(&cpu_idle_count, relaxed);
	}
}
