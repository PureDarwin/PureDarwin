/*
 * Copyright (c) 2007-2024 Apple Inc. All rights reserved.
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
#include <debug.h>
#include <mach_ldebug.h>
#include <mach_kdp.h>

#include <kern/misc_protos.h>
#include <kern/thread.h>
#include <kern/timer_queue.h>
#include <kern/processor.h>
#include <kern/startup.h>
#include <kern/debug.h>
#include <kern/monotonic.h>
#include <prng/random.h>
#include <machine/machine_routines.h>
#include <machine/commpage.h>
#include <machine/config.h>
#include <pexpert/riscv/boot.h>
#include <pexpert/riscv/board_config.h>
#include <pexpert/device_tree.h>
#include <riscv/proc_reg.h>
#include <riscv/pmap.h>
#include <riscv/caches_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/machine_cpu.h>
#include <riscv/rtclock.h>
#include <vm/vm_map.h>

#include <libkern/kernel_mach_header.h>
#include <libkern/stack_protector.h>
#include <libkern/section_keywords.h>
#include <san/kasan.h>
#include <sys/kdebug.h>

#include <pexpert/pexpert.h>

#include <console/serial_protos.h>

#if CONFIG_TELEMETRY
#include <kern/telemetry.h>
#endif

extern void     patch_low_glo(void);
extern int      serial_init(void);

extern vm_offset_t intstack_top;
extern vm_offset_t excepstack_top;

extern const char version[];
extern const char version_variant[];
extern int      disableConsoleOutput;

int             debug_task;

TUNABLE(bool, restore_boot, "-restore", false);

#if SCHED_HYGIENE_DEBUG
#define DEFAULT_INTERRUPT_MASKED_TIMEOUT 0xd0000 /* 35.499ms at 24MHz */

TUNABLE_DT_WRITEABLE(sched_hygiene_mode_t, interrupt_masked_debug_mode,
    "machine-timeouts", "interrupt-masked-debug-mode",
    "interrupt-masked-debug-mode",
    SCHED_HYGIENE_MODE_PANIC,
    TUNABLE_DT_CHECK_CHOSEN);

MACHINE_TIMEOUT_DEV_WRITEABLE(interrupt_masked_timeout, "interrupt-masked",
    DEFAULT_INTERRUPT_MASKED_TIMEOUT, MACHINE_TIMEOUT_UNIT_TIMEBASE,
    NULL);
MACHINE_TIMEOUT_DEV_WRITEABLE(stackshot_interrupt_masked_timeout, "sshot-interrupt-masked",
    0xf9999, MACHINE_TIMEOUT_UNIT_TIMEBASE,
    NULL);
#endif

#define XCALL_ACK_TIMEOUT_NS ((uint64_t) 6000000000)
uint64_t xcall_ack_timeout_abstime;

#ifndef __BUILDING_XNU_LIBRARY__
#define BOOTARGS_SECTION_ATTR __attribute__((section("__DATA, __const")))
#else /* __BUILDING_XNU_LIBRARY__ */
#define BOOTARGS_SECTION_ATTR
#endif /* __BUILDING_XNU_LIBRARY__ */

boot_args const_boot_args BOOTARGS_SECTION_ATTR;
boot_args      *BootArgs BOOTARGS_SECTION_ATTR;

extern boolean_t force_immediate_debug_halt;

SECURITY_READ_ONLY_LATE(uint64_t) gDramBase;
SECURITY_READ_ONLY_LATE(uint64_t) gDramSize;
SECURITY_READ_ONLY_LATE(ppnum_t)  pmap_first_pnum;

SECURITY_READ_ONLY_LATE(bool) serial_console_enabled = false;

// the hart the loader entered on, ml_parse_cpu_topology maps it to a cpu number
SECURITY_READ_ONLY_LATE(uint32_t) riscv_boot_hartid;

void riscv_init(boot_args * args, uint32_t hartid);
void riscv_init_cpu(cpu_data_t *cpu_data_ptr) __attribute__((noreturn));

#define dyldLogFunc(msg, ...)
#include <mach/dyld_kernel_fixups.h>

extern vm_offset_t vm_kernel_slide;
extern vm_offset_t segLOWESTKC, segHIGHESTKC, segLOWESTROKC, segHIGHESTROKC;
extern vm_offset_t segLOWESTAuxKC, segHIGHESTAuxKC, segLOWESTROAuxKC, segHIGHESTROAuxKC;
extern vm_offset_t segLOWESTRXAuxKC, segHIGHESTRXAuxKC, segHIGHESTNLEAuxKC;

// nothing before this may use an absolute pointer, the collection's chained fixups are still unapplied
static void
riscv_slide_rebase_image(void)
{
	kernel_mach_header_t *k_mh, *kc_mh = NULL;
	kernel_segment_command_t *seg;
	uintptr_t slide;

	k_mh = &_mh_execute_header;
	if (!kernel_mach_header_is_in_fileset(k_mh)) {
		// a riscv kernel only ever boots from a fileset kernel collection
		for (;;) {
			__asm__ volatile ("wfi");
		}
	}

	// the first load command is the kernel's __TEXT, its vmaddr gives the slide
	seg = (kernel_segment_command_t *)((uintptr_t)k_mh + sizeof(*k_mh));
	slide = (uintptr_t)k_mh - seg->vmaddr;

	// the collection's own header sits at the fixed link address
	kc_mh = (kernel_mach_header_t*)(VM_KERNEL_LINK_ADDRESS + slide);

	const void *collection_base_pointers[KCNumKinds] = {[0] = kc_mh, };
	kernel_collection_slide((struct mach_header_64 *)kc_mh, collection_base_pointers);

	PE_set_kc_header(KCKindPrimary, kc_mh, slide);

	// the loader leaves load command vmaddrs unslid
	kernel_collection_adjust_mh_addrs((struct mach_header_64 *)kc_mh, slide, false,
	    (uintptr_t *)&segLOWESTKC, (uintptr_t *)&segHIGHESTKC,
	    (uintptr_t *)&segLOWESTROKC, (uintptr_t *)&segHIGHESTROKC,
	    NULL, NULL, NULL);

	vm_kernel_slide = slide;
}

void riscv_static_if_init(boot_args *args);
MARK_AS_FIXUP_TEXT void
riscv_static_if_init(boot_args *args)
{
	static_if_init(args->CommandLine);
}

void
riscv_auxkc_init(void *mh, void *base)
{
	// the lowest vmaddr in an auxiliary collection is 0, its header sits above the rw segments
	uintptr_t slide = (uintptr_t)base;
	kernel_mach_header_t *akc_mh = (kernel_mach_header_t*)mh;

	assert(akc_mh->filetype == MH_FILESET);
	PE_set_kc_header_and_base(KCKindAuxiliary, akc_mh, base, slide);

	const void *collection_base_pointers[KCNumKinds];
	memcpy(collection_base_pointers, PE_get_kc_base_pointers(), sizeof(collection_base_pointers));
	kernel_collection_slide((struct mach_header_64 *)akc_mh, collection_base_pointers);

	kernel_collection_adjust_mh_addrs((struct mach_header_64 *)akc_mh, slide, false,
	    (uintptr_t *)&segLOWESTAuxKC, (uintptr_t *)&segHIGHESTAuxKC, (uintptr_t *)&segLOWESTROAuxKC,
	    (uintptr_t *)&segHIGHESTROAuxKC, (uintptr_t *)&segLOWESTRXAuxKC, (uintptr_t *)&segHIGHESTRXAuxKC,
	    (uintptr_t *)&segHIGHESTNLEAuxKC);
}

// Routine: riscv_init
// Function: Runs on the boot hart, once, on entry from the loader.
__startup_func
void
riscv_init(
	boot_args       *args,
	uint32_t        hartid)
{
	unsigned int    maxmem;
	uint32_t        memsize;
	uint64_t        xmaxmem;
	thread_t        thread;
	DTEntry chosen = NULL;
	unsigned int dt_entry_size = 0;
	int             boot_cpu_id;

	riscv_slide_rebase_image();

	riscv_static_if_init(args);

	const_boot_args = *args;
	BootArgs = args = &const_boot_args;
	riscv_boot_hartid = hartid;

	cpu_data_init(&BootCpuData);

	PE_init_platform(FALSE, args); /* Get platform expert set up */

	ml_parse_cpu_topology();

	boot_cpu_id = ml_get_boot_cpu_number();
	assert(boot_cpu_id >= 0 && boot_cpu_id <= ml_get_max_cpu_number());

	BootCpuData.cpu_number = (unsigned short)boot_cpu_id;
	BootCpuData.cpu_phys_id = hartid;
	BootCpuData.intstack_top = (vm_offset_t) &intstack_top;
	BootCpuData.istackptr = &intstack_top;
	BootCpuData.excepstack_top = (vm_offset_t) &excepstack_top;
	BootCpuData.excepstackptr = &excepstack_top;
	CpuDataEntries[boot_cpu_id].cpu_data_vaddr = &BootCpuData;
	CpuDataEntries[boot_cpu_id].cpu_data_paddr = (void *)((uintptr_t)(args->physBase)
	    + ((uintptr_t)&BootCpuData
	    - (uintptr_t)(args->virtBase)));

	thread = thread_bootstrap();
	thread->machine.CpuDatap = &BootCpuData;
	thread->machine.pcpu_data_base_and_cpu_number =
	    ml_make_pcpu_base_and_cpu_number(0, BootCpuData.cpu_number);
	machine_set_current_thread(thread);

	// scheduling isn't running yet, preemption can stay enabled so mutexes work
	thread->machine.preemption_count = 0;
	cpu_bootstrap();

	rtclock_early_init();

	kernel_debug_string_early("kernel_startup_bootstrap");
	kernel_startup_bootstrap();

	timer_call_init();

	cpu_init();

	processor_bootstrap();

	if (PE_parse_boot_argn("maxmem", &maxmem, sizeof(maxmem))) {
		xmaxmem = (uint64_t) maxmem * (1024 * 1024);
	} else if (PE_get_default("hw.memsize", &memsize, sizeof(memsize))) {
		xmaxmem = (uint64_t) memsize;
	} else {
		xmaxmem = 0;
	}

#if SCHED_HYGIENE_DEBUG
	{
		int wdt_boot_arg = 0;
		bool const wdt_disabled = (PE_parse_boot_argn("wdt", &wdt_boot_arg, sizeof(wdt_boot_arg)) && (wdt_boot_arg == -1));

		if (wdt_disabled || kern_feature_override(KF_INTERRUPT_MASKED_DEBUG_OVRD)) {
			interrupt_masked_debug_mode = SCHED_HYGIENE_MODE_OFF;
		}
		if (wdt_disabled || kern_feature_override(KF_PREEMPTION_DISABLED_DEBUG_OVRD)) {
			sched_preemption_disable_debug_mode = SCHED_HYGIENE_MODE_OFF;
		}
	}
#endif /* SCHED_HYGIENE_DEBUG */

	nanoseconds_to_absolutetime(XCALL_ACK_TIMEOUT_NS, &xcall_ack_timeout_abstime);

	PE_parse_boot_argn("immediate_NMI", &force_immediate_debug_halt, sizeof(force_immediate_debug_halt));

	// the loader reports the real dram range, gPhysBase/Size only cover kernel managed memory
	unsigned long const *dram_base;
	unsigned long const *dram_size;

	if (SecureDTLookupEntry(NULL, "/chosen", &chosen) != kSuccess) {
		panic("%s: Unable to find 'chosen' DT node", __FUNCTION__);
	}

	if (SecureDTGetProperty(chosen, "dram-base", (void const **)&dram_base, &dt_entry_size) != kSuccess) {
		panic("%s: Unable to find 'dram-base' entry in the 'chosen' DT node", __FUNCTION__);
	}

	if (SecureDTGetProperty(chosen, "dram-size", (void const **)&dram_size, &dt_entry_size) != kSuccess) {
		panic("%s: Unable to find 'dram-size' entry in the 'chosen' DT node", __FUNCTION__);
	}

	gDramBase = *dram_base;
	gDramSize = *dram_size;
	pmap_first_pnum = (ppnum_t)atop(gDramBase);

	riscv_vm_init(xmaxmem, args);

	if (debug_boot_arg) {
		patch_low_glo();
	}

	/* Setup debugging output. */
	const unsigned int serial_exists = serial_init();
	kernel_startup_initialize_upto(STARTUP_SUB_KPRINTF);
	kprintf("kprintf initialized\n");

	serialmode = 0;
	if (PE_parse_boot_argn("serial", &serialmode, sizeof(serialmode))) {
		/* Do we want a serial keyboard and/or console? */
		kprintf("Serial mode specified: %08X\n", serialmode);
		disable_iolog_serial_output = (serialmode & SERIALMODE_NO_IOLOG) != 0;
		enable_dklog_serial_output = restore_boot || (serialmode & SERIALMODE_DKLOG) != 0;
		int force_sync = serialmode & SERIALMODE_SYNCDRAIN;
		if (force_sync || PE_parse_boot_argn("drain_uart_sync", &force_sync, sizeof(force_sync))) {
			if (force_sync) {
				serialmode |= SERIALMODE_SYNCDRAIN;
				kprintf(
					"WARNING: Forcing uart driver to output synchronously."
					"printf()s/IOLogs will impact kernel performance.\n"
					"You are advised to avoid using 'drain_uart_sync' boot-arg.\n");
			}
		}
	}
	if (kern_feature_override(KF_SERIAL_OVRD)) {
		serialmode = 0;
	}

	/* Start serial if requested and a serial device was enumerated in serial_init(). */
	if ((serialmode & SERIALMODE_OUTPUT) && serial_exists) {
		serial_console_enabled = true;
		(void)switch_to_serial_console(); /* Switch into serial mode from video console */
		disableConsoleOutput = FALSE;     /* Allow printfs to happen */
	}
	PE_create_console();

	/* setup console output */
	PE_init_printf(FALSE);

	cpu_machine_idle_init(TRUE);

	PE_init_platform(TRUE, &BootCpuData);

	cpu_timebase_init(TRUE);

	PE_init_cpu();

	// the stack protector guard for everything after this, a zero byte stops string overruns
	__stack_chk_guard = (unsigned long)early_random();
	__stack_chk_guard &= ~(0xFFULL << 8);
	machine_startup(args);
}

// Routine: riscv_init_cpu
// Function: Runs on secondary harts started through sbi hart_start, and on resume.
void
riscv_init_cpu(
	cpu_data_t      *cpu_data_ptr)
{
	// everything below reaches per-cpu state through the current thread, so tp comes first
	machine_set_current_thread(cpu_data_ptr->cpu_active_thread);

	// start.s put the hart on the kernel tables, no user tables yet
	pmap_clear_user_ttb();

	os_atomic_andnot(&cpu_data_ptr->cpu_flags, SleepState, relaxed);

	cpu_machine_idle_init(FALSE);

	cpu_init();

	// the timebase comes before serial_init, some serial drivers rate limit with mach_absolute_time
	cpu_timebase_init(FALSE);

	if (cpu_data_ptr == &BootCpuData && ml_is_quiescing()) {
		serial_init();
		PE_init_platform(TRUE, NULL);
		commpage_update_timebase();
	}
	PE_init_cpu();

	cpu_data_ptr->rtcPop = EndOfAllTime;
	timer_resync_deadlines();

	processor_t processor = PERCPU_GET_RELATIVE(processor, cpu_data, cpu_data_ptr);
	bool should_kprintf = processor_should_kprintf(processor, true);

	if (should_kprintf) {
		kprintf("riscv_init_cpu(): cpu %d online\n", cpu_data_ptr->cpu_number);
	}

	if (cpu_data_ptr == &BootCpuData && ml_is_quiescing()) {
		if (kdebug_enable == 0) {
			__kdebug_only uint64_t elapsed = kdebug_wake();
			KDBG(IOKDBG_CODE(DBG_HIBERNATE, 15), mach_absolute_time() - elapsed);
		}
	}

	secondary_cpu_main(NULL);
}

// Routine: riscv_init_idle_cpu
// Function: Resume an idle hart whose state was lost while it slept.
void __attribute__((noreturn))
riscv_init_idle_cpu(
	cpu_data_t      *cpu_data_ptr)
{
	machine_set_current_thread(cpu_data_ptr->cpu_active_thread);
	pmap_clear_user_ttb();
	cpu_idle_exit(TRUE);
}
