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
#ifndef RISCV_CPU_DATA_INTERNAL
#define RISCV_CPU_DATA_INTERNAL

#include <mach_assert.h>
#include <kern/assert.h>
#include <kern/kern_types.h>
#include <kern/percpu.h>
#include <kern/processor.h>
#include <os/base.h>
#include <pexpert/pexpert.h>
#include <riscv/machine_routines.h>
#include <riscv/thread.h>
#include <riscv/proc_reg.h>
#include <riscv/pmap.h>
#include <san/kcov_data.h>

#define NSEC_PER_HZ     (NSEC_PER_SEC / 100)

/* Put the static check for cpumap_t here as it's defined in <kern/processor.h> */
static_assert(sizeof(cpumap_t) * CHAR_BIT >= MAX_CPUS, "cpumap_t bitvector is too small for current MAX_CPUS value");

// per cpu copy windows sit in the last 2MB megapage below the top of the kernel range
#define CPUWINDOWS_BASE_MASK            0xFFFFFFFFFFE00000UL
#define CPUWINDOWS_BASE                 (VM_MAX_KERNEL_ADDRESS & CPUWINDOWS_BASE_MASK)
#define CPUWINDOWS_TOP                  (CPUWINDOWS_BASE + (MAX_CPUS * CPUWINDOWS_MAX * PAGE_SIZE))

#ifndef __BUILDING_XNU_LIBRARY__
static_assert((CPUWINDOWS_BASE >= VM_MIN_KERNEL_ADDRESS) && ((CPUWINDOWS_TOP - 1) <= VM_MAX_KERNEL_ADDRESS),
    "CPU copy windows too large for CPUWINDOWS_BASE_MASK value");
#endif

typedef struct cpu_data_entry {
	void                           *cpu_data_paddr;         /* Cpu data physical address */
	struct  cpu_data               *cpu_data_vaddr;         /* Cpu data virtual address */
} cpu_data_entry_t;

typedef struct rtclock_timer {
	mpqueue_head_t                  queue;
	uint64_t                        deadline;
	uint32_t                        is_set:1,
	    has_expired:1,
	:0;
} rtclock_timer_t;

typedef struct {
	// the wake variants are reset to 0 when the cpu wakes
	uint64_t irq_ex_cnt;
	uint64_t irq_ex_cnt_wake;
	uint64_t ipi_cnt;
	uint64_t ipi_cnt_wake;
	uint64_t timer_cnt;
	uint64_t undef_ex_cnt;
	uint64_t unaligned_cnt;
	uint64_t vfp_cnt;
	uint64_t data_ex_cnt;
	uint64_t instr_ex_cnt;
} cpu_stat_t;

__options_closed_decl(cpu_flags_t, uint16_t, {
	SleepState      = 0x0800,
	/* For the boot processor, StartedState means 'interrupts initialized' - it is already running */
	StartedState    = 0x1000,
	/* For the boot processor, InitState means 'cpu_data fully initialized' - it is already running */
	InitState       = 0x2000,
});

__options_closed_decl(cpu_signal_t, unsigned int, {
	SIGPnop         = 0x00000000U,     /* Send IPI with no service */
	SIGPMaintenance = 0x00000001U,     /* SMR, Ledger flush        */
	SIGPxcall       = 0x00000004U,     /* Call a function on a processor */
	SIGPast         = 0x00000008U,     /* Request AST check */
	SIGPdebug       = 0x00000010U,     /* Request Debug call */
	SIGPLWFlush     = 0x00000020U,     /* Request LWFlush call */
	SIGPLWClean     = 0x00000040U,     /* Request LWClean call */
	SIGPkppet       = 0x00000100U,     /* Request kperf PET handler */
	SIGPxcallImm    = 0x00000200U,     /* Send a cross-call, fail if already pending */
	SIGPTimerLocal  = 0x00000400U,     /* Update the decrementer via timer_queue_expire_local */
	SIGPdeferred    = 0x00000800U,     /* Scheduler deferred IPI to wake core */
#if DEVELOPMENT || DEBUG
	SIGPtest        = 0x00002000U,     /* Test signal for panic testing */
#endif

	SIGPdisabled    = 0x80000000U,     /* Signal disabled */
});

// the trap vector reaches this through gp, keep the first fields in step with the assembly
typedef struct cpu_data {
	unsigned short                  cpu_number;
	_Atomic cpu_flags_t             cpu_flags;
	int                             cpu_type;
	int                             cpu_subtype;
	int                             cpu_threadtype;

	void *                          istackptr;
	vm_offset_t                     intstack_top;
	void *                          excepstackptr;
	vm_offset_t                     excepstack_top;
	thread_t                        cpu_active_thread;
	vm_offset_t                     cpu_active_stack;
	cpu_id_t                        cpu_id;
	volatile cpu_signal_t           cpu_signal;
	ast_t                           cpu_pending_ast;
	cache_dispatch_t                cpu_cache_dispatch;

	uint64_t                        cpu_base_timebase;
	uint64_t                        cpu_timebase;
	bool                            cpu_hibernate;
	bool                            cpu_running;
	bool                            cluster_master;
	bool                            sync_on_cswitch;
	/* true if processor_start() or processor_exit() is operating on this CPU */
	bool                            in_state_transition;

	uint32_t                        cpu_decrementer;
	get_decrementer_t               cpu_get_decrementer_func;
	set_decrementer_t               cpu_set_decrementer_func;

	processor_idle_t                cpu_idle_notify;
	uint64_t                        cpu_idle_latency;
	uint64_t                        cpu_idle_pop;

	unsigned int                    interrupt_source;
	void                            *cpu_int_state;
	IOInterruptHandler              interrupt_handler;
	void                            *interrupt_nub;
	void                            *interrupt_target;
	void                            *interrupt_refCon;

	idle_timer_t                    idle_timer_notify;
	void                            *idle_timer_refcon;
	uint64_t                        idle_timer_deadline;

	uint64_t                        rtcPop;
	rtclock_timer_t                 rtclock_timer;
	struct _rtclock_data_           *rtclock_datap;

	volatile int                    debugger_active;
	volatile int                    PAB_active; /* Tells the console if we are dumping backtraces */

	void                            *cpu_xcall_p0;
	void                            *cpu_xcall_p1;
	void                            *cpu_imm_xcall_p0;
	void                            *cpu_imm_xcall_p1;

	uint32_t                        cpu_phys_id;            /* hart id */
	platform_error_handler_t        platform_error_handler;

	int                             cpu_mcount_off;

	volatile unsigned int           cpu_sleep_token;
	unsigned int                    cpu_sleep_token_last;

	cluster_type_t                  cpu_cluster_type;

	uint32_t                        cpu_cluster_id;
	uint32_t                        cpu_l2_id;
	uint32_t                        cpu_l2_size;
	uint32_t                        cpu_l3_id;
	uint32_t                        cpu_l3_size;

	enum {
		CPU_NOT_HALTED = 0,
		CPU_HALTED,
		CPU_HALTED_WITH_STATE
	}                               halt_status;

	cpu_stat_t                      cpu_stat;
	struct pmap_cpu_data            cpu_pmap_cpu_data;
#if CONFIG_KCOV
	kcov_cpu_data_t                 cpu_kcov_data;
#endif

	// the state of the hart when an ipi arrived, dumped if that ends in a panic
	uint64_t                        ipi_pc;
	uint64_t                        ipi_lr;
	uint64_t                        ipi_fp;
} cpu_data_t;

extern  cpu_data_entry_t                CpuDataEntries[MAX_CPUS];
PERCPU_DECL(cpu_data_t, cpu_data);
#define BootCpuData                     __PERCPU_NAME(cpu_data)
extern  boot_args                      *BootArgs;

extern cpu_data_t      *cpu_datap(int cpu);
extern cpu_data_t      *cpu_data_alloc(boolean_t is_boot);
extern void             cpu_stack_alloc(cpu_data_t*);
extern void             cpu_data_init(cpu_data_t *cpu_data_ptr);
extern void             cpu_data_register(cpu_data_t *cpu_data_ptr);
extern cpu_data_t      *processor_to_cpu_datap( processor_t processor);

#endif  /* RISCV_CPU_DATA_INTERNAL */
