/*
 * Copyright (c) 2007-2020 Apple Inc. All rights reserved.
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
#include <mach_kdp.h>
#include <kern/kern_stackshot.h>

#include <kern/thread.h>
#include <machine/pmap.h>
#include <device/device_types.h>

#include <mach/vm_param.h>
#include <mach/clock_types.h>
#include <mach/machine.h>
#include <mach/kmod.h>
#include <pexpert/boot.h>
#include <pexpert/pexpert.h>

#include <ptrauth.h>

#include <kern/ecc.h>
#include <kern/misc_protos.h>
#include <kern/startup.h>
#include <kern/clock.h>
#include <kern/debug.h>
#include <kern/processor.h>
#include <kdp/kdp_core.h>
#include <machine/atomic.h>
#include <machine/trap.h>
#include <kern/spl.h>
#include <pexpert/pexpert.h>
#include <kdp/kdp_callout.h>
#include <kdp/kdp_dyld.h>
#include <kdp/kdp_internal.h>
#include <kdp/kdp_common.h>
#include <uuid/uuid.h>
#include <sys/codesign.h>
#include <sys/time.h>


#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOKitServer.h>

#include <mach/vm_prot.h>
#include <vm/vm_map_xnu.h>
#include <vm/pmap.h>
#include <vm/vm_shared_region.h>
#include <mach/time_value.h>
#include <machine/machparam.h>  /* for btop */

#include <console/video_console.h>
#include <console/serial_protos.h>
#include <riscv/cpu_data.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/machine_cpu.h>
#include <libkern/OSKextLibPrivate.h>
#include <vm/vm_kern.h>
#include <kern/kern_cdata.h>
#include <kern/ledger.h>


#if DEVELOPMENT || DEBUG
#include <kern/ext_paniclog.h>
#endif

#if CONFIG_EXCLAVES
#include <kern/exclaves_panic.h>
#include <kern/exclaves_inspection.h>
#endif


#if     MACH_KDP
void    kdp_trap(unsigned int, struct riscv_saved_state *);
#endif

// Increment the PANICLOG_VERSION if you change the format of the panic log in any way.
#define PANICLOG_VERSION 15
static struct kcdata_descriptor kc_panic_data;

extern char iBoot_version[];

extern volatile uint32_t        debug_enabled;
extern unsigned int         not_in_kdp;

extern void                             kdp_callouts(kdp_event_t event);

#define MAX_PROCNAME_LEN 32
/* #include <sys/proc.h> */
struct proc;
extern int        proc_pid(struct proc *p);
extern void       proc_name_kdp(struct proc *, char *, int);

// Must fit the panic header's version string space, in the OSAnalytics/DumpPanic format
// 'Product Version (OS Version)'.
#define PANIC_HEADER_VERSION_FMT_STR "%.14s (%.14s)"

extern const char version[];
extern char       osversion[];
extern char       osproductversion[];
extern char       osreleasetype[];

#if defined(XNU_TARGET_OS_BRIDGE)
extern char     macosproductversion[];
extern char     macosversion[];
#endif

extern uint8_t          gPlatformECID[8];
extern uint32_t         gPlatformMemoryID;

extern uint64_t         last_hwaccess_thread;
extern uint8_t          last_hwaccess_type; /* 0 : read, 1 : write. */
extern uint8_t          last_hwaccess_size;
extern uint64_t         last_hwaccess_paddr;
#if HAS_SPTM_SYSCTL
extern bool             disarm_protected_io;
extern bool             disarm_protected_io_ever;
#endif /* HAS_SPTM_SYSCTL */

/* Choosing the size for gTargetTypeBuffer as 16 since the target
 * name typically doesn't exceed this size */
extern char  gTargetTypeBuffer[16];
/* Device-specific buffers for coalesced targets to show actual device info instead of coalesced info */
extern char  gUniqueDeviceTargetTypeBuffer[16];
extern char  gUniqueDeviceModelTypeBuffer[32];

extern struct timeval    gIOLastSleepTime;
extern struct timeval    gIOLastWakeTime;
extern boolean_t                 is_clock_configured;
extern boolean_t kernelcache_uuid_valid;
extern uuid_t kernelcache_uuid;
extern uuid_string_t bootsessionuuid_string;

extern uint64_t roots_installed;

/* Definitions for frame pointers */
// s0 points just above the frame record, the return address sits at fp - 8 and the caller's fp at fp - 16
#define FP_ALIGNMENT_MASK      ((uint32_t)(0x7))
#define FP_RA_OFFSET           ((uint64_t)8)
#define FP_PREV_FP_OFFSET      ((uint64_t)16)
#define FP_MAX_NUM_TO_EVALUATE (50)

/* Timeout for all processors responding to debug crosscall */
MACHINE_TIMEOUT_ALWAYS_ENABLED(debug_ack_timeout, "debug-ack", 240000, MACHINE_TIMEOUT_UNIT_TIMEBASE);

/* Forward functions definitions */
void panic_display_times(void);
void panic_print_symbol_name(vm_address_t search);


/* Global variables */
static uint32_t       panic_bt_depth;
boolean_t             PanicInfoSaved = FALSE;
boolean_t             force_immediate_debug_halt = FALSE;
unsigned int          debug_ack_timeout_count = 0;
_Atomic unsigned int  debugger_sync = 0;
_Atomic unsigned int  mp_kdp_trap = 0; /* CPUs signalled by the debug CPU will spin on this */
_Atomic unsigned int  debug_cpus_spinning = 0; /* Number of signalled CPUs still spinning on mp_kdp_trap (in DebuggerXCall). */
unsigned int          DebugContextCount = 0;
bool                  trap_is_stackshot = false; /* Whether the trap is for a stackshot */



// reads one 64-bit word of a stack through the physical aperture, so a bad frame never faults
static bool
read_frame_word(pmap_t pmap, addr64_t va, addr64_t *value)
{
	ppnum_t ppn = pmap_find_phys(pmap, va);

	if (ppn == 0) {
		return false;
	}
	*value = ml_phys_read_double_64(((((addr64_t)ppn) << PAGE_SHIFT)) | (va & PAGE_MASK));
	return true;
}

static void
do_print_backtrace_internal(pmap_t pmap, vm_offset_t topfp, const char *cur_marker,
    boolean_t print_kexts_in_backtrace)
{
	unsigned int    i = 0;
	addr64_t        lr = 0;
	addr64_t        fp = topfp;
	vm_offset_t     raddrs[FP_MAX_NUM_TO_EVALUATE] = { 0 };
	bool            dump_kernel_stack = (fp >= VM_MIN_KERNEL_ADDRESS);

	do {
		if ((fp == 0) || ((fp & FP_ALIGNMENT_MASK) != 0)) {
			break;
		}

		if ((!dump_kernel_stack) && (fp >= VM_MIN_KERNEL_ADDRESS)) {
			break;
		}

		if (!read_frame_word(pmap, fp - FP_RA_OFFSET, &lr)) {
			paniclog_append_noflush("%s\t  Could not read LR from frame at 0x%016llx\n", cur_marker, fp - FP_RA_OFFSET);
			break;
		}
		if (!read_frame_word(pmap, fp - FP_PREV_FP_OFFSET, &fp)) {
			paniclog_append_noflush("%s\t  Could not read FP from frame at 0x%016llx\n", cur_marker, fp - FP_PREV_FP_OFFSET);
			break;
		}
		// i may == FP_MAX_NUM_TO_EVALUATE on the extra round that checks for a complete
		// backtrace, capture nothing then.
		if (i < FP_MAX_NUM_TO_EVALUATE && lr) {
			paniclog_append_noflush("%s\t  lr: 0x%016llx  fp: 0x%016llx\n", cur_marker, lr, fp);
			raddrs[i] = lr;
		}
	} while ((++i <= FP_MAX_NUM_TO_EVALUATE) && (fp != topfp));

	if (i > FP_MAX_NUM_TO_EVALUATE && fp != 0) {
		paniclog_append_noflush("Backtrace continues...\n");
	}

	if (print_kexts_in_backtrace && i > 0) {
		kmod_panic_dump(&raddrs[0], i);
	}
}

#define SANE_TASK_LIMIT 256
#define TOP_RUNNABLE_LIMIT 5
#define PANICLOG_UUID_BUF_SIZE 256

extern void panic_print_vnodes(void);

static void
panic_display_pvhs_locked(void)
{
}

static void
panic_display_pvh_to_lock(void)
{
}

static void
panic_display_last_pc_lr(void)
{
	const int max_cpu = ml_get_max_cpu_number();

	for (int cpu = 0; cpu <= max_cpu; cpu++) {
		cpu_data_t *current_cpu_datap = cpu_datap(cpu);

		if (current_cpu_datap == NULL) {
			continue;
		}

		if (current_cpu_datap == getCpuDatap()) {
			// Skip printing the PC/LR if this is the CPU that initiated the panic.
			paniclog_append_noflush("CORE %u is the one that panicked. Check the full backtrace for details.\n", cpu);
			continue;
		}

		paniclog_append_noflush("CORE %u: PC=0x%016llx, LR=0x%016llx, FP=0x%016llx\n", cpu,
		    current_cpu_datap->ipi_pc, (uint64_t)VM_KERNEL_STRIP_PTR(current_cpu_datap->ipi_lr),
		    (uint64_t)VM_KERNEL_STRIP_PTR(current_cpu_datap->ipi_fp));
	}
}

#if CONFIG_EXCLAVES
static void
panic_report_exclaves_stackshot(void)
{
	if (exclaves_panic_ss_status == EXCLAVES_PANIC_STACKSHOT_FOUND) {
		paniclog_append_noflush("** Exclaves panic stackshot found\n");
	} else if (exclaves_panic_ss_status == EXCLAVES_PANIC_STACKSHOT_NOT_FOUND) {
		paniclog_append_noflush("** Exclaves panic stackshot not found\n");
	} else if (exclaves_panic_ss_status == EXCLAVES_PANIC_STACKSHOT_DECODE_FAILED) {
		paniclog_append_noflush("!! Exclaves panic stackshot decode failed !!\n");
	}
}
#endif /* CONFIG_EXCLAVES */

__attribute__((always_inline))
static inline void
print_backtrace_internal(thread_t thread, bool filesetKC)
{
	uintptr_t cur_fp = (uintptr_t)__builtin_frame_address(0);
	const char              *nohilite_thread_marker = "\t";

	if (cur_fp < VM_MAX_KERNEL_ADDRESS) {
		paniclog_append_noflush("Panicked thread: %p, backtrace: 0x%llx, tid: %llu\n",
		    thread, (addr64_t)cur_fp, thread_tid(thread));
		do_print_backtrace_internal(kernel_pmap, cur_fp, nohilite_thread_marker, filesetKC);
	} else {
		paniclog_append_noflush("Could not print panicked thread backtrace:"
		    "frame pointer outside kernel vm.\n");
	}
}

static bool
is_filesetKC(void)
{
	kc_format_t     kc_format;
	bool            filesetKC = false;

	__unused bool result = PE_get_primary_kc_format(&kc_format);
	assert(result == true);
	filesetKC = kc_format == KCFormatFileset;
	return filesetKC;
}


static void
do_print_all_panic_info(const char *message, uint64_t panic_options, const char *panic_initiator)
{
	int             logversion = PANICLOG_VERSION;
	thread_t        cur_thread = current_thread();
	task_t          task;
	struct proc    *proc;
	int             print_vnodes = 0;

	/* end_marker_bytes set to 200 for printing END marker + stackshot summary info always */
	int bytes_traced = 0, bytes_remaining = 0, end_marker_bytes = 200;
	int bytes_uncompressed = 0;
	uint64_t bytes_used = 0ULL;
	int err = 0;
	char *stackshot_begin_loc = NULL;
	bool filesetKC = is_filesetKC();
	uint32_t panic_initiator_len = 0;
#if CONFIG_EXT_PANICLOG
	uint32_t ext_paniclog_bytes = 0;
#endif

	if (panic_bt_depth != 0) {
		return;
	}
	panic_bt_depth++;

	/* Truncate panic string to 1200 bytes */
	paniclog_append_noflush("Debugger message: %.1200s\n", message);
	if (debug_enabled) {
		paniclog_append_noflush("Device: %s\n",
		    ('\0' != gUniqueDeviceTargetTypeBuffer[0]) ? gUniqueDeviceTargetTypeBuffer : "Not set yet");
		paniclog_append_noflush("Hardware Model: %s\n",
		    ('\0' != gUniqueDeviceModelTypeBuffer[0]) ? gUniqueDeviceModelTypeBuffer:"Not set yet");
		paniclog_append_noflush("ECID: %02X%02X%02X%02X%02X%02X%02X%02X\n", gPlatformECID[7],
		    gPlatformECID[6], gPlatformECID[5], gPlatformECID[4], gPlatformECID[3],
		    gPlatformECID[2], gPlatformECID[1], gPlatformECID[0]);
#if HAS_SPTM_SYSCTL
		if (disarm_protected_io_ever) {
			paniclog_append_noflush("SPTM protected IO disabled? %d, ever disabled? %d\n",
			    disarm_protected_io, disarm_protected_io_ever);
		}
#endif /* HAS_SPTM_SYSCTL */
		if (last_hwaccess_thread) {
			paniclog_append_noflush("AppleHWAccess Thread: 0x%llx\n", last_hwaccess_thread);
			if (!last_hwaccess_size) {
				paniclog_append_noflush("AppleHWAccess last access: no access data, this is unexpected.\n");
			} else {
				const char *typ = last_hwaccess_type ? "write" : "read";
				paniclog_append_noflush("AppleHWAccess last access: %s of size %u at address 0x%llx\n", typ, last_hwaccess_size, last_hwaccess_paddr);
			}
		}
		paniclog_append_noflush("Boot args: %s\n", PE_boot_args());
	}
	paniclog_append_noflush("Memory ID: 0x%x\n", gPlatformMemoryID);
	paniclog_append_noflush("OS release type: %.256s\n",
	    ('\0' != osreleasetype[0]) ? osreleasetype : "Not set yet");
	paniclog_append_noflush("OS version: %.256s\n",
	    ('\0' != osversion[0]) ? osversion : "Not set yet");
#if defined(XNU_TARGET_OS_BRIDGE)
	paniclog_append_noflush("macOS version: %.256s\n",
	    ('\0' != macosversion[0]) ? macosversion : "Not set");
#endif
	paniclog_append_noflush("Kernel version: %.512s\n", version);

#if CONFIG_EXCLAVES
	exclaves_panic_append_info();
#endif

	if (kernelcache_uuid_valid) {
		if (filesetKC) {
			paniclog_append_noflush("Fileset Kernelcache UUID: ");
		} else {
			paniclog_append_noflush("KernelCache UUID: ");
		}
		for (size_t index = 0; index < sizeof(uuid_t); index++) {
			paniclog_append_noflush("%02X", kernelcache_uuid[index]);
		}
		paniclog_append_noflush("\n");
	}
	panic_display_kernel_uuid();

	if (bootsessionuuid_string[0] != '\0') {
		paniclog_append_noflush("Boot session UUID: %s\n", bootsessionuuid_string);
	} else {
		paniclog_append_noflush("Boot session UUID not yet initialized\n");
	}

	paniclog_append_noflush("iBoot version: %.128s\n", iBoot_version);

	paniclog_append_noflush("secure boot?: %s\n", debug_enabled ? "NO": "YES");
	paniclog_append_noflush("roots installed: %lld\n", roots_installed);
	if (panic_data_buffers != NULL) {
		paniclog_append_noflush("%s data: ", panic_data_buffers->producer_name);
		uint8_t *panic_buffer_data = (uint8_t *) panic_data_buffers->buf;
		for (int i = 0; i < panic_data_buffers->len; i++) {
			paniclog_append_noflush("%02X", panic_buffer_data[i]);
		}
		paniclog_append_noflush("\n");
	}
	paniclog_append_noflush("Paniclog version: %d\n", logversion);

	panic_display_kernel_aslr();
	panic_display_times();
	panic_display_zalloc();


	panic_display_pvhs_locked();
	panic_display_pvh_to_lock();
	panic_display_last_pc_lr();
#if CONFIG_ECC_LOGGING
	panic_display_ecc_errors();
#endif /* CONFIG_ECC_LOGGING */
	panic_display_compressor_stats();

#if DEVELOPMENT || DEBUG
	if (cs_debug_unsigned_exec_failures != 0 || cs_debug_unsigned_mmap_failures != 0) {
		paniclog_append_noflush("Unsigned code exec failures: %u\n", cs_debug_unsigned_exec_failures);
		paniclog_append_noflush("Unsigned code mmap failures: %u\n", cs_debug_unsigned_mmap_failures);
	}
#endif

	// Highlight threads that used high amounts of CPU in the panic log if requested (historically requested for watchdog panics)
	if (panic_options & DEBUGGER_OPTION_PRINT_CPU_USAGE_PANICLOG) {
		thread_t        top_runnable[5] = {0};
		thread_t        thread;
		int                     total_cpu_usage = 0;

		print_vnodes = 1;


		for (thread = (thread_t)queue_first(&threads);
		    PANIC_VALIDATE_PTR(thread) && !queue_end(&threads, (queue_entry_t)thread);
		    thread = (thread_t)queue_next(&thread->threads)) {
			total_cpu_usage += thread->cpu_usage;

			// Look for the 5 runnable threads with highest priority
			if (thread->state & TH_RUN) {
				int                     k;
				thread_t        comparison_thread = thread;

				for (k = 0; k < TOP_RUNNABLE_LIMIT; k++) {
					if (top_runnable[k] == 0) {
						top_runnable[k] = comparison_thread;
						break;
					} else if (comparison_thread->sched_pri > top_runnable[k]->sched_pri) {
						thread_t temp = top_runnable[k];
						top_runnable[k] = comparison_thread;
						comparison_thread = temp;
					} // if comparison thread has higher priority than previously saved thread
				} // loop through highest priority runnable threads
			} // Check if thread is runnable
		} // Loop through all threads

		// Print the relevant info for each thread identified
		paniclog_append_noflush("Total cpu_usage: %d\n", total_cpu_usage);
		paniclog_append_noflush("Thread task pri cpu_usage\n");

		for (int i = 0; i < TOP_RUNNABLE_LIMIT; i++) {
			if (top_runnable[i] &&
			    panic_get_thread_proc_task(top_runnable[i], &task, &proc) && proc) {
				char name[MAX_PROCNAME_LEN + 1];
				proc_name_kdp(proc, name, sizeof(name));
				paniclog_append_noflush("%p %s %d %d\n",
				    top_runnable[i], name, top_runnable[i]->sched_pri, top_runnable[i]->cpu_usage);
			}
		} // Loop through highest priority runnable threads
		paniclog_append_noflush("\n");
	}

	// print current task info
	if (panic_get_thread_proc_task(cur_thread, &task, &proc)) {
		if (PANIC_VALIDATE_PTR(task->map) &&
		    PANIC_VALIDATE_PTR(task->map->pmap)) {
			ledger_amount_t resident = 0;
			if (task != kernel_task) {
				ledger_get_balance(task->ledger, task_ledgers.phys_mem,
				    LEO_NO_SETTLE, &resident);
				resident >>= VM_MAP_PAGE_SHIFT(task->map);
			}
			paniclog_append_noflush("Panicked task %p: %lld pages, %d threads: ",
			    task, resident, task->thread_count);
		} else {
			paniclog_append_noflush("Panicked task %p: %d threads: ",
			    task, task->thread_count);
		}

		if (proc) {
			char            name[MAX_PROCNAME_LEN + 1];
			proc_name_kdp(proc, name, sizeof(name));
			paniclog_append_noflush("pid %d: %s", proc_pid(proc), name);
		} else {
			paniclog_append_noflush("unknown task");
		}

		paniclog_append_noflush("\n");
	}

	print_backtrace_internal(cur_thread, filesetKC);

	paniclog_append_noflush("\n");
	dump_cpu_event_log(&paniclog_append_noflush);

	paniclog_append_noflush("\n");
	if (filesetKC) {
		kext_dump_panic_lists(&paniclog_append_noflush);
		paniclog_append_noflush("\n");
	}
	panic_info->eph_panic_log_len = PE_get_offset_into_panic_region(debug_buf_ptr) - panic_info->eph_panic_log_offset;
	/* set the os version data in the panic header in the format 'Product Version (OS Version)' (only if they have been set) */
	if ((osversion[0] != '\0') && (osproductversion[0] != '\0')) {
		snprintf((char *)&panic_info->eph_os_version, sizeof(panic_info->eph_os_version), PANIC_HEADER_VERSION_FMT_STR,
		    osproductversion, osversion);
	}
#if defined(XNU_TARGET_OS_BRIDGE)
	if ((macosversion[0] != '\0') && (macosproductversion[0] != '\0')) {
		snprintf((char *)&panic_info->eph_macos_version, sizeof(panic_info->eph_macos_version), PANIC_HEADER_VERSION_FMT_STR,
		    macosproductversion, macosversion);
	}
#endif
	if (bootsessionuuid_string[0] != '\0') {
		memcpy(panic_info->eph_bootsessionuuid_string, bootsessionuuid_string,
		    sizeof(panic_info->eph_bootsessionuuid_string));
	}
	panic_info->eph_roots_installed = roots_installed;

	/* Copy device-specific target and model type buffers */
	memcpy(panic_info->eph_device_target_type, gUniqueDeviceTargetTypeBuffer, sizeof(panic_info->eph_device_target_type));
	memcpy(panic_info->eph_device_model_type, gUniqueDeviceModelTypeBuffer, sizeof(panic_info->eph_device_model_type));

	if (panic_initiator != NULL) {
		bytes_remaining = debug_buf_size - (unsigned int)((uintptr_t)debug_buf_ptr - (uintptr_t)debug_buf_base);
		// If panic_initiator isn't null, safely copy up to MAX_PANIC_INITIATOR_SIZE
		panic_initiator_len = strnlen(panic_initiator, MAX_PANIC_INITIATOR_SIZE);
		// Calculate the bytes to write, accounting for remaining buffer space, and ensuring the lowest size we can have is 0
		panic_initiator_len = MAX(0, MIN(panic_initiator_len, bytes_remaining));
		panic_info->eph_panic_initiator_offset = (panic_initiator_len != 0) ? PE_get_offset_into_panic_region(debug_buf_ptr) : 0;
		panic_info->eph_panic_initiator_len = panic_initiator_len;
		memcpy(debug_buf_ptr, panic_initiator, panic_initiator_len);
		debug_buf_ptr += panic_initiator_len;
	}

	if (debug_ack_timeout_count) {
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_DEBUGGERSYNC;
		panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		paniclog_append_noflush("!! debugger synchronization failed, no stackshot !!\n");
	} else if (panic_stackshot_active()) {
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_NESTED;
		panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		paniclog_append_noflush("!! panicked during stackshot, skipping panic stackshot !!\n");
	} else {
		/* Align the stackshot buffer to an 8-byte address (especially important for armv7k devices) */
		debug_buf_ptr += (8 - ((uintptr_t)debug_buf_ptr % 8));
		stackshot_begin_loc = debug_buf_ptr;

		bytes_remaining = debug_buf_size - (unsigned int)((uintptr_t)stackshot_begin_loc - (uintptr_t)debug_buf_base);
		err = kcdata_memory_static_init(&kc_panic_data, (mach_vm_address_t)debug_buf_ptr,
		    KCDATA_BUFFER_BEGIN_COMPRESSED, bytes_remaining - end_marker_bytes,
		    KCFLAG_USE_MEMCOPY);
		if (err == KERN_SUCCESS) {
			uint64_t stackshot_flags = (STACKSHOT_GET_GLOBAL_MEM_STATS | STACKSHOT_SAVE_LOADINFO | STACKSHOT_KCDATA_FORMAT |
			    STACKSHOT_ENABLE_BT_FAULTING | STACKSHOT_ENABLE_UUID_FAULTING | STACKSHOT_FROM_PANIC | STACKSHOT_DO_COMPRESS |
			    STACKSHOT_DISABLE_LATENCY_INFO | STACKSHOT_NO_IO_STATS | STACKSHOT_THREAD_WAITINFO | STACKSHOT_GET_DQ |
			    STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT);

			err = kcdata_init_compress(&kc_panic_data, KCDATA_BUFFER_BEGIN_STACKSHOT, kdp_memcpy, KCDCT_ZLIB);
			if (err != KERN_SUCCESS) {
				panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COMPRESS_FAILED;
				stackshot_flags &= ~STACKSHOT_DO_COMPRESS;
			}
			if (filesetKC) {
				stackshot_flags |= STACKSHOT_SAVE_KEXT_LOADINFO;
			}

			kdp_snapshot_preflight(-1, stackshot_begin_loc, bytes_remaining - end_marker_bytes,
			    stackshot_flags, &kc_panic_data, 0, 0);
			err = do_panic_stackshot(NULL);
			bytes_traced = kdp_stack_snapshot_bytes_traced();
			if (bytes_traced > 0 && !err) {
				debug_buf_ptr += bytes_traced;
				panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_SUCCEEDED;
				panic_info->eph_stackshot_offset = PE_get_offset_into_panic_region(stackshot_begin_loc);
				panic_info->eph_stackshot_len = bytes_traced;

				panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
#if CONFIG_EXCLAVES
				panic_report_exclaves_stackshot();
#endif /* CONFIG_EXCLAVES */
				if (stackshot_flags & STACKSHOT_DO_COMPRESS) {
					panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_DATA_COMPRESSED;
					bytes_uncompressed = kdp_stack_snapshot_bytes_uncompressed();
					paniclog_append_noflush("\n** Stackshot Succeeded ** Bytes Traced %d (Uncompressed %d) **\n", bytes_traced, bytes_uncompressed);
				} else {
					paniclog_append_noflush("\n** Stackshot Succeeded ** Bytes Traced %d **\n", bytes_traced);
				}
			} else {
				bytes_used = kcdata_memory_get_used_bytes(&kc_panic_data);
#if CONFIG_EXCLAVES
				panic_report_exclaves_stackshot();
#endif /* CONFIG_EXCLAVES */
				if (bytes_used > 0) {
					/* Zero out the stackshot data */
					bzero(stackshot_begin_loc, bytes_used);
					panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_INCOMPLETE;

					panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
					paniclog_append_noflush("\n** Stackshot Incomplete ** Bytes Filled %llu, err %d **\n", bytes_used, err);
				} else {
					bzero(stackshot_begin_loc, bytes_used);
					panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR;

					panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
					paniclog_append_noflush("\n!! Stackshot Failed !! Bytes Traced %d, err %d\n", bytes_traced, err);
				}
			}
		} else {
			panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR;
			panic_info->eph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
			paniclog_append_noflush("\n!! Stackshot Failed !!\nkcdata_memory_static_init returned %d", err);
		}
	}

#if CONFIG_EXT_PANICLOG
	// Write ext paniclog at the end of the paniclog region.
	ext_paniclog_bytes = ext_paniclog_write_panicdata();
	panic_info->eph_ext_paniclog_offset = (ext_paniclog_bytes != 0) ?
	    PE_get_offset_into_panic_region((debug_buf_base + debug_buf_size) - ext_paniclog_bytes) :
	    0;
	panic_info->eph_ext_paniclog_len = ext_paniclog_bytes;
#endif

	assert(panic_info->eph_other_log_offset != 0);

	if (print_vnodes != 0) {
		panic_print_vnodes();
	}

	panic_bt_depth--;
}

// Entry to print_all_panic_info is serialized by the debugger lock
static void
print_all_panic_info(const char *message, uint64_t panic_options, const char *panic_initiator)
{
	unsigned int initial_not_in_kdp = not_in_kdp;

	cpu_data_t * cpu_data_ptr = getCpuDatap();

	assert(cpu_data_ptr->PAB_active == FALSE);
	cpu_data_ptr->PAB_active = TRUE;

	// Printing all backtraces uses pmap routines, which must avoid pmap locks,
	// conditionalized on not_in_kdp.
	not_in_kdp = 0;
	do_print_all_panic_info(message, panic_options, panic_initiator);

	not_in_kdp = initial_not_in_kdp;

	cpu_data_ptr->PAB_active = FALSE;
}

void
print_curr_backtrace(void)
{
	print_backtrace_internal(current_thread(), is_filesetKC());
}

void
panic_display_times()
{
	if (kdp_clock_is_locked()) {
		paniclog_append_noflush("Warning: clock is locked.  Can't get time\n");
		return;
	}

	extern lck_ticket_t clock_lock;
	extern lck_grp_t clock_lock_grp;

	if ((is_clock_configured) && (lck_ticket_lock_try(&clock_lock, &clock_lock_grp))) {
		clock_sec_t     secs, boot_secs;
		clock_usec_t    usecs, boot_usecs;

		lck_ticket_unlock(&clock_lock);

		clock_get_calendar_microtime(&secs, &usecs);
		clock_get_boottime_microtime(&boot_secs, &boot_usecs);

		paniclog_append_noflush("mach_absolute_time: 0x%llx\n", mach_absolute_time());
		paniclog_append_noflush("Epoch Time:        sec       usec\n");
		paniclog_append_noflush("  Boot    : 0x%08x 0x%08x\n", (unsigned int)boot_secs, (unsigned int)boot_usecs);
		paniclog_append_noflush("  Sleep   : 0x%08x 0x%08x\n", (unsigned int)gIOLastSleepTime.tv_sec, (unsigned int)gIOLastSleepTime.tv_usec);
		paniclog_append_noflush("  Wake    : 0x%08x 0x%08x\n", (unsigned int)gIOLastWakeTime.tv_sec, (unsigned int)gIOLastWakeTime.tv_usec);
		paniclog_append_noflush("  Calendar: 0x%08x 0x%08x\n\n", (unsigned int)secs, (unsigned int)usecs);
	}
}

void
panic_print_symbol_name(vm_address_t search)
{
#pragma unused(search)
	// empty stub. Really only used on x86_64.
	return;
}

void
SavePanicInfo(
	const char *message, __unused void *panic_data, uint64_t panic_options, const char* panic_initiator)
{
	// If not initialized yet, loop here rather than assert, so early-boot panics don't recurse.
	// A PC here means the real early-boot panic is a few frames down.
	while (!panic_info || panic_info->eph_panic_log_offset == 0) {
		// rdar://87170225 (PanicHardening: audit panic code for naked spinloops)
		// rdar://88094367 (Add test hooks for panic at different stages in XNU)
		;
	}

	if (panic_options & DEBUGGER_OPTION_PANICLOGANDREBOOT) {
		panic_info->eph_panic_flags  |= EMBEDDED_PANIC_HEADER_FLAG_BUTTON_RESET_PANIC;
	}

	if (panic_options & DEBUGGER_OPTION_COMPANION_PROC_INITIATED_PANIC) {
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COMPANION_PROC_INITIATED_PANIC;
	}

	if (panic_options & DEBUGGER_OPTION_INTEGRATED_COPROC_INITIATED_PANIC) {
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_INTEGRATED_COPROC_INITIATED_PANIC;
	}

	if (panic_options & DEBUGGER_OPTION_USERSPACE_INITIATED_PANIC) {
		panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_USERSPACE_INITIATED_PANIC;
	}


	// Panic data may live directly in the iBoot panic region. On re-entry (double panic),
	// update the panic CRC so iBoot can find something useful there.
	if (PanicInfoSaved && (debug_buf_base >= (char*)gPanicBase) && (debug_buf_base < (char*)gPanicBase + gPanicSize)) {
		unsigned int pi_size = (unsigned int)(debug_buf_ptr - gPanicBase);
		PE_update_panic_crc((unsigned char*)gPanicBase, &pi_size);
		PE_sync_panic_buffers(); // extra precaution; panic path likely isn't reliable if we're here
	}

	if (PanicInfoSaved || (debug_buf_size == 0)) {
		return;
	}

	PanicInfoSaved = TRUE;


	print_all_panic_info(message, panic_options, panic_initiator);

	assert(panic_info->eph_panic_log_len != 0);
	panic_info->eph_other_log_len = PE_get_offset_into_panic_region(debug_buf_ptr) - panic_info->eph_other_log_offset;

	PEHaltRestart(kPEPanicSync);

	// Notifies registered IOPlatformPanicAction callbacks (including the memcache disable)
	// and flushes the buffer contents from the cache
	paniclog_flush();
}

void
paniclog_flush()
{
	unsigned int panicbuf_length = 0;

	panicbuf_length = (unsigned int)(debug_buf_ptr - gPanicBase);
	if (!debug_buf_ptr || !panicbuf_length) {
		return;
	}

	// Updates the log length of the last part of the panic log.
	panic_info->eph_other_log_len = PE_get_offset_into_panic_region(debug_buf_ptr) - panic_info->eph_other_log_offset;

	// Updates the metadata at the beginning of the panic buffer, updates the CRC.
	PE_update_panic_crc((unsigned char *)gPanicBase, &panicbuf_length);

	// Unused by platform KEXTs on embedded, kept for the published IOKit interfaces.
	PESavePanicInfo((unsigned char *)gPanicBase, panicbuf_length);

	PE_sync_panic_buffers();
}

// IPI other cores into a busy loop so this core runs single-threaded, debugger lock held.
// Returns KERN_OPERATION_TIMED_OUT on sync timeout unless proceed_on_sync_failure (unsafe).
kern_return_t
DebuggerXCallEnter(
	boolean_t proceed_on_sync_failure, bool is_stackshot)
{
	extern _Atomic(unsigned int) panic_stop_count;
	uint64_t max_mabs_time, current_mabs_time;
	int cpu;
	int timeout_cpu = -1;
	int max_cpu;
	unsigned int sync_pending;
	cpu_data_t      *target_cpu_datap;
	cpu_data_t      *cpu_data_ptr = getCpuDatap();

	/* Check for nested debugger entry. */
	cpu_data_ptr->debugger_active++;
	if (cpu_data_ptr->debugger_active != 1) {
		return KERN_SUCCESS;
	}

	// A nonzero debugger_sync means someone acked the last request very late. Zero it again
	// so one timeout doesn't make every later debugger entry time out.

	os_atomic_store(&debugger_sync, 0, relaxed);
	os_atomic_store(&mp_kdp_trap, 1, relaxed);
	os_atomic_store(&debug_cpus_spinning, 0, relaxed);
	trap_is_stackshot = is_stackshot;


	// Signal all other CPUs, debugger_sync tracks every CPU that appeared to be signalled
	// since cpu_signal is not synchronous.
	max_cpu = ml_get_max_cpu_number();

	boolean_t immediate_halt = FALSE;
	if (proceed_on_sync_failure && force_immediate_debug_halt) {
		immediate_halt = TRUE;
	}

	if (!immediate_halt) {
		for (cpu = 0; cpu <= max_cpu; cpu++) {
			target_cpu_datap = (cpu_data_t *)CpuDataEntries[cpu].cpu_data_vaddr;

			if ((target_cpu_datap == NULL) || (target_cpu_datap == cpu_data_ptr)) {
				continue;
			}

			kern_return_t ret = cpu_signal(target_cpu_datap, SIGPdebug, (void *)NULL, NULL);
			if (ret == KERN_SUCCESS) {
				os_atomic_inc(&debugger_sync, relaxed);
				os_atomic_inc(&debug_cpus_spinning, relaxed);
			} else {
				kprintf("%s: cpu_signal failed. cpu=%d ret=%d proceed=%d\n", __func__, cpu, ret, proceed_on_sync_failure);
			}
		}

		max_mabs_time = os_atomic_load(&debug_ack_timeout, relaxed);

		// A zero debug_ack_timeout waits one second, enough for VMs. Waiting forever caused watchdog
		// timeouts when CPUs were stuck with interrupts disabled.
		current_mabs_time = mach_absolute_time();
		max_mabs_time = current_mabs_time + ((max_mabs_time > 0) ? max_mabs_time : 24000000ull);
		assert(max_mabs_time > current_mabs_time);

		// Wait DEBUG_ACK_TIMEOUT ns for everyone we IPI'd. Without a true NMI a timeout is tolerated,
		// hoping the rest are spinning in a debugger-safe context.
		do {
			current_mabs_time = mach_absolute_time();
			sync_pending = os_atomic_load(&debugger_sync, acquire) -
			    os_atomic_load(&panic_stop_count, acquire);
		} while ((sync_pending != 0) && (current_mabs_time < max_mabs_time));
	}

	if (!immediate_halt && current_mabs_time >= max_mabs_time) {
		// Timed out IPIing the other CPUs. Skip offline CPUs, then count the remainder as halted
		// if the system is going down anyway.
		__asm__ volatile ("fence rw, rw" ::: "memory");
		for (cpu = 0; cpu <= max_cpu; cpu++) {
			target_cpu_datap = (cpu_data_t *)CpuDataEntries[cpu].cpu_data_vaddr;

			if ((target_cpu_datap == NULL) || (target_cpu_datap == cpu_data_ptr)) {
				continue;
			}
			if (!(target_cpu_datap->cpu_signal & SIGPdebug)) {
				continue;
			}
			if (processor_array[cpu]->state <= PROCESSOR_PENDING_OFFLINE) {
				int dbg_sync_count;

				// Signalled with SIGPdebug but went offline with interrupts disabled before
				// the IPI arrived, so count it as halted.
				dbg_sync_count = os_atomic_dec(&debugger_sync, relaxed);
				kprintf("%s>found CPU %d offline, debugger_sync=%d\n", __FUNCTION__, cpu, dbg_sync_count);
				continue;
			}
			kprintf("%s>Debugger synch pending on cpu %d\n", __FUNCTION__, cpu);
			timeout_cpu = cpu;
		}

		sync_pending = os_atomic_load(&debugger_sync, acquire) - os_atomic_load(&panic_stop_count, acquire);
		if (sync_pending == 0) {
			return KERN_SUCCESS;
		} else if (!proceed_on_sync_failure) {
			panic("%s>Debugger synch pending on cpu %d\n",
			    __FUNCTION__, timeout_cpu);
		}
	}
	if (immediate_halt || (current_mabs_time >= max_mabs_time)) {
		// no debug halt unit exists, a hart that did not answer keeps running where it is
		__asm__ volatile ("fence rw, rw" ::: "memory");
		for (cpu = 0; cpu <= max_cpu; cpu++) {
			target_cpu_datap = (cpu_data_t *)CpuDataEntries[cpu].cpu_data_vaddr;

			if ((target_cpu_datap == NULL) || (target_cpu_datap == cpu_data_ptr)) {
				continue;
			}
			if (!immediate_halt && !(target_cpu_datap->cpu_signal & SIGPdebug)) {
				continue;
			}
			paniclog_append_noflush("cpu %d did not stop for the debugger\n", cpu);
			debug_ack_timeout_count++;
		}
		if (immediate_halt) {
			paniclog_append_noflush("Immediate halt requested on all cores\n");
		} else {
			paniclog_append_noflush("Debugger synchronization timed out; timeout %llu nanoseconds\n",
			    os_atomic_load(&debug_ack_timeout, relaxed));
		}
	}
	return KERN_SUCCESS;
}

// Resume normal multicore operation after DebuggerXCallEnter(), debugger lock held.
void
DebuggerXCallReturn(
	void)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();
	uint64_t max_mabs_time, current_mabs_time;

	cpu_data_ptr->debugger_active--;
	if (cpu_data_ptr->debugger_active != 0) {
		return;
	}

	os_atomic_store(&mp_kdp_trap, 0, release);
	os_atomic_store(&debugger_sync, 0, relaxed);

	max_mabs_time = os_atomic_load(&debug_ack_timeout, relaxed);

	if (max_mabs_time > 0) {
		current_mabs_time = mach_absolute_time();
		max_mabs_time += current_mabs_time;
		assert(max_mabs_time > current_mabs_time);
	}

	// Wait DEBUG_ACK_TIMEOUT ns for other CPUs to leave mp_kdp_trap and move on regardless,
	// some may be stuck elsewhere with interrupts disabled (same as DebuggerXCallEnter).
	while ((os_atomic_load(&debug_cpus_spinning, acquire) != 0) &&
	    (max_mabs_time == 0 || current_mabs_time < max_mabs_time)) {
		cpu_pause();
		current_mabs_time = mach_absolute_time();
	}

	// checking debug_ack_timeout != 0 is a workaround for rdar://124242354
	if (current_mabs_time >= max_mabs_time && os_atomic_load(&debug_ack_timeout, relaxed) != 0) {
		panic("Resuming from debugger synchronization failed: waited %llu nanoseconds\n", os_atomic_load(&debug_ack_timeout, relaxed));
	}
}

extern void wait_while_mp_kdp_trap(bool check_SIGPdebug);
// Spin while mp_kdp_trap is set. With check_SIGPdebug (from processor_offline()),
// break out if the cpu has SIGPdebug pending.
void
wait_while_mp_kdp_trap(bool check_SIGPdebug)
{
	bool found_mp_kdp_trap = false;
	bool found_SIGPdebug = false;

	while (os_atomic_load(&mp_kdp_trap, acquire) != 0) {
		found_mp_kdp_trap = true;
		if (check_SIGPdebug && cpu_has_SIGPdebug_pending()) {
			found_SIGPdebug = true;
			break;
		}
		cpu_pause();
	}

	if (check_SIGPdebug && found_mp_kdp_trap) {
		kprintf("%s>found_mp_kdp_trap=true found_SIGPdebug=%s\n", __FUNCTION__, found_SIGPdebug ? "true" : "false");
	}
}

void
DebuggerXCall(
	void            *ctx)
{
	boolean_t               save_context = FALSE;
	vm_offset_t             kstackptr = 0;
	riscv_saved_state_t     *regs = (riscv_saved_state_t *) ctx;

	if (regs != NULL) {
		current_cpu_datap()->ipi_pc = (uint64_t)get_saved_state_pc(regs);
		current_cpu_datap()->ipi_lr = (uint64_t)get_saved_state_lr(regs);
		current_cpu_datap()->ipi_fp = (uint64_t)get_saved_state_fp(regs);
		// sstatus.spp is set when the ipi interrupted the kernel
		save_context = (regs->sstatus & SSTATUS_SPP) != 0;
	}

	kstackptr = (vm_offset_t)current_thread()->machine.kstackptr;

	riscv_kernel_saved_state_t *state = (riscv_kernel_saved_state_t *)kstackptr;

	if (save_context) {
		/* Save the interrupted context before acknowledging the signal */
		current_thread()->machine.kpcb = regs;
	} else if (regs) {
		/* zero old state so machine_trace_thread knows not to backtrace it */
		state->s[0] = 0;
		state->ra = 0;
		state->sp = 0;
	}

	// Serial-mode dumps may hold interrupts off past the timeout. Check other cores' timeout
	// before spinning, and reset the timestamp after to avoid the interrupt timeout assert().
	if ((serialmode & SERIALMODE_OUTPUT) || trap_is_stackshot) {
		ml_interrupt_masked_debug_end();
	}

	// Do stackshot preflight before decrementing debugger sync, signalling availability
	// before the stackshot-calling CPU starts work.

	if (trap_is_stackshot) {
		stackshot_cpu_preflight();
	}

	os_atomic_dec(&debugger_sync, release);

	/* If we trapped because we're doing a stackshot, do our work first. */
	if (trap_is_stackshot) {
		stackshot_aux_cpu_entry();
	}


	wait_while_mp_kdp_trap(false);

	// Tell the triggering CPU this CPU is done spinning, DebuggerXCallReturn waits
	// for all CPUs to exit the loop above.
	os_atomic_dec(&debug_cpus_spinning, release);

#if SCHED_HYGIENE_DEBUG
	// Also abandon the preemption disable measurement. Ending the interrupt handler early would
	// attribute the spin to preemption disable time and could trigger a panic.
	abandon_preemption_disable_measurement();

	if ((serialmode & SERIALMODE_OUTPUT) || trap_is_stackshot) {
		ml_interrupt_masked_debug_start((void *)current_thread()->machine.int_handler_addr, current_thread()->machine.int_type);
	}
#endif /* SCHED_HYGIENE_DEBUG */

	current_thread()->machine.kpcb = NULL;

	/* Any cleanup for our pushed context should go here */
}

void
DebuggerCall(
	unsigned int    reason,
	void            *ctx)
{
#if     !MACH_KDP
#pragma unused(reason,ctx)
#endif /* !MACH_KDP */

#if     MACH_KDP
	kdp_trap(reason, (struct riscv_saved_state *)ctx);
#else
	/* TODO: decide what to do if no debugger config */
#endif
}

boolean_t
bootloader_valid_page(ppnum_t ppn)
{
	return pmap_bootloader_page(ppn);
}

// kprintf reaches the console once riscv_init has run serial_init and the kprintf startup
boolean_t
PE_arm_debug_and_trace_initialized(void)
{
	return startup_phase >= STARTUP_SUB_KPRINTF;
}
