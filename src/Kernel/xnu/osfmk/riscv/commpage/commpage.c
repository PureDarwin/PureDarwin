/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * @APPLE_FREE_COPYRIGHT@
 */
// File: riscv/commpage/commpage.c, sets up and exports a RO/RW page
#include <libkern/section_keywords.h>
#include <mach/mach_types.h>
#include <mach/machine.h>
#include <mach/vm_map.h>
#include <machine/cpu_capabilities.h>
#include <machine/commpage.h>
#include <machine/config.h>
#include <machine/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_protos.h>
#include <ipc/ipc_port.h>
#include <riscv/misc_protos.h>
#include <riscv/rtclock.h>
#include <libkern/OSAtomic.h>
#include <stdatomic.h>
#include <kern/remote_time.h>
#include <kern/smr.h>
#include <machine/atomic.h>
#include <machine/machine_remote_time.h>
#include <machine/machine_routines.h>
#include <sys/code_signing.h>

#include <sys/kdebug.h>
#include <sys/random.h>

#if CONFIG_ATM
#include <atm/atm_internal.h>
#endif

static int commpage_cpus( void );

#include <riscv/proc_reg.h>


static void commpage_init_cpu_capabilities( void );

SECURITY_READ_ONLY_LATE(vm_address_t)   commPagePtr = 0;
SECURITY_READ_ONLY_LATE(vm_address_t)   commpage_rw_addr = 0;
SECURITY_READ_ONLY_LATE(vm_address_t)   commpage_kernel_ro_addr = 0;
SECURITY_READ_ONLY_LATE(uint64_t)       _cpu_capabilities = 0;
SECURITY_READ_ONLY_LATE(vm_address_t)   commpage_rw_text_addr = 0;

extern user64_addr_t commpage_text64_location;
extern user32_addr_t commpage_text32_location;

// from machine_routines.c
extern bool ml_riscv_isa_has(const char *ext);
extern void ml_map_cpus_to_clusters(uint8_t *table);
extern unsigned int ml_get_cluster_count(void);

void
commpage_populate(void)
{
	uint16_t        c2;
	int cpufamily;

	// Create the data and the text commpage
	vm_map_address_t kernel_data_addr, kernel_text_addr, kernel_ro_data_addr, user_text_addr;
	pmap_create_commpages(&kernel_data_addr, &kernel_text_addr, &kernel_ro_data_addr, &user_text_addr);

	commpage_rw_addr = kernel_data_addr;
	commpage_rw_text_addr = kernel_text_addr;
	commpage_kernel_ro_addr = kernel_ro_data_addr;
	commPagePtr = (vm_address_t) _COMM_PAGE_BASE_ADDRESS;

	commpage_text64_location = user_text_addr;
	bcopy(_COMM_PAGE64_SIGNATURE_STRING, (void *)(_COMM_PAGE_SIGNATURE + _COMM_PAGE_RW_OFFSET),
	    MIN(_COMM_PAGE_SIGNATURELEN, strlen(_COMM_PAGE64_SIGNATURE_STRING)));

	*((uint16_t*)(_COMM_PAGE_VERSION + _COMM_PAGE_RW_OFFSET)) = (uint16_t) _COMM_PAGE_THIS_VERSION;

	commpage_init_cpu_capabilities();
	commpage_set_timestamp(0, 0, 0, 0, 0);

	if (_cpu_capabilities & kCache32) {
		c2 = 32;
	} else if (_cpu_capabilities & kCache64) {
		c2 = 64;
	} else if (_cpu_capabilities & kCache128) {
		c2 = 128;
	} else {
		c2 = 0;
	}

	*((uint16_t*)(_COMM_PAGE_CACHE_LINESIZE + _COMM_PAGE_RW_OFFSET)) = c2;

	commpage_update_active_cpus();
	// no riscv cpu families are defined yet
	cpufamily = CPUFAMILY_UNKNOWN;
	*((uint8_t*)(_COMM_PAGE_CPU_CLUSTERS + _COMM_PAGE_RW_OFFSET)) = (uint8_t) ml_get_cluster_count();
	*((uint8_t*)(_COMM_PAGE_PHYSICAL_CPUS + _COMM_PAGE_RW_OFFSET)) = (uint8_t) machine_info.physical_cpu_max;
	*((uint8_t*)(_COMM_PAGE_LOGICAL_CPUS + _COMM_PAGE_RW_OFFSET)) = (uint8_t) machine_info.logical_cpu_max;
	*((uint64_t*)(_COMM_PAGE_MEMORY_SIZE + _COMM_PAGE_RW_OFFSET)) = machine_info.max_mem;
	*((uint32_t*)(_COMM_PAGE_CPUFAMILY + _COMM_PAGE_RW_OFFSET)) = (uint32_t)cpufamily;
	*((uint32_t*)(_COMM_PAGE_DEV_FIRM_LEGACY + _COMM_PAGE_RW_OFFSET)) = (uint32_t)PE_i_can_has_debugger(NULL);
	*((uint32_t*)(_COMM_PAGE_DEV_FIRM + _COMM_PAGE_RO_OFFSET)) = (uint32_t)PE_i_can_has_debugger(NULL);
	*((uint8_t*)(_COMM_PAGE_USER_TIMEBASE + _COMM_PAGE_RW_OFFSET)) = user_timebase_type();

	// Populate logical CPU -> logical cluster table
	ml_map_cpus_to_clusters((uint8_t*)(_COMM_PAGE_CPU_TO_CLUSTER + _COMM_PAGE_RW_OFFSET));

	*((uint8_t*)(_COMM_PAGE_CONT_HWCLOCK + _COMM_PAGE_RW_OFFSET)) = (uint8_t)user_cont_hwclock_allowed();
	*((uint8_t*)(_COMM_PAGE_KERNEL_PAGE_SHIFT_LEGACY + _COMM_PAGE_RW_OFFSET)) = (uint8_t) page_shift;
	*((uint8_t*)(_COMM_PAGE_KERNEL_PAGE_SHIFT + _COMM_PAGE_RO_OFFSET)) = (uint8_t) page_shift;

	*((uint8_t*)(_COMM_PAGE_USER_PAGE_SHIFT_32_LEGACY + _COMM_PAGE_RW_OFFSET)) = (uint8_t) page_shift;
	*((uint8_t*)(_COMM_PAGE_USER_PAGE_SHIFT_32 + _COMM_PAGE_RO_OFFSET)) = (uint8_t) page_shift;
	*((uint8_t*)(_COMM_PAGE_USER_PAGE_SHIFT_64_LEGACY + _COMM_PAGE_RW_OFFSET)) = (uint8_t) page_shift;
	*((uint8_t*)(_COMM_PAGE_USER_PAGE_SHIFT_64 + _COMM_PAGE_RO_OFFSET)) = (uint8_t) page_shift;

	commpage_update_timebase();
	commpage_update_mach_continuous_time(0);

	clock_sec_t secs;
	clock_usec_t microsecs;
	clock_get_boottime_microtime(&secs, &microsecs);
	commpage_update_boottime(secs * USEC_PER_SEC + microsecs);

	// set commpage approximate time to zero for initialization,
	// the scheduler populates the correct value before running a user thread
	*((uint64_t *)(_COMM_PAGE_APPROX_TIME + _COMM_PAGE_RW_OFFSET)) = 0;
#ifdef CONFIG_MACH_APPROXIMATE_TIME
	*((uint8_t *)(_COMM_PAGE_APPROX_TIME_SUPPORTED + _COMM_PAGE_RW_OFFSET)) = 1;
#else
	*((uint8_t *)(_COMM_PAGE_APPROX_TIME_SUPPORTED + _COMM_PAGE_RW_OFFSET)) = 0;
#endif

	commpage_update_kdebug_state();

#if CONFIG_ATM
	commpage_update_atm_diagnostic_config(atm_get_diagnostic_config());
#endif


	*((uint64_t*)(_COMM_PAGE_REMOTETIME_PARAMS + _COMM_PAGE_RW_OFFSET)) = BT_RESET_SENTINEL_TS;

#if CONFIG_QUIESCE_COUNTER
	cpu_quiescent_set_storage((_Atomic uint64_t *)(_COMM_PAGE_CPU_QUIESCENT_COUNTER +
	    _COMM_PAGE_RW_OFFSET));
#endif /* CONFIG_QUIESCE_COUNTER */

	// Set random values for Apple Security Bounty targets. addr should be unmapped for
	// userland processes, kaddr unmapped for the kernel.
	uint64_t asb_value, asb_addr, asb_kvalue, asb_kaddr;
	uint64_t asb_rand_vals[] = {
		0x93e78adcded4d3d5, 0xd16c5b76ad99bccf, 0x67dfbbd12c4a594e, 0x7365636e6f6f544f,
		0x239a974c9811e04b, 0xbf60e7fa45741446, 0x8acf5210b466b05, 0x67dfbbd12c4a594e
	};
	const int nrandval = sizeof(asb_rand_vals) / sizeof(asb_rand_vals[0]);
	uint8_t randidx;

	read_random(&randidx, sizeof(uint8_t));
	asb_value = asb_rand_vals[randidx++ % nrandval];
	*((uint64_t*)(_COMM_PAGE_ASB_TARGET_VALUE + _COMM_PAGE_RW_OFFSET)) = asb_value;

	// userspace faulting address should be > MACH_VM_MAX_ADDRESS
	asb_addr = asb_rand_vals[randidx++ % nrandval];
	uint64_t user_min = MACH_VM_MAX_ADDRESS;
	uint64_t user_max = UINT64_MAX;
	asb_addr %= (user_max - user_min);
	asb_addr += user_min;
	*((uint64_t*)(_COMM_PAGE_ASB_TARGET_ADDRESS + _COMM_PAGE_RW_OFFSET)) = asb_addr;

	asb_kvalue = asb_rand_vals[randidx++ % nrandval];
	*((uint64_t*)(_COMM_PAGE_ASB_TARGET_KERN_VALUE + _COMM_PAGE_RW_OFFSET)) = asb_kvalue;

	// kernel faulting address should be < VM_MIN_KERNEL_ADDRESS
	asb_kaddr = asb_rand_vals[randidx++ % nrandval];
	uint64_t kernel_min = 0x0LL;
	uint64_t kernel_max = VM_MIN_KERNEL_ADDRESS;
	asb_kaddr %= (kernel_max - kernel_min);
	asb_kaddr += kernel_min;
	*((uint64_t*)(_COMM_PAGE_ASB_TARGET_KERN_ADDRESS + _COMM_PAGE_RW_OFFSET)) = asb_kaddr;

	commpage_set_erm_active(extended_research_mode_state());
}

#define COMMPAGE_TEXT_SEGMENT "__TEXT_EXEC"
#define COMMPAGE_TEXT_SECTION "__commpage_text"

/* Get a pointer to the start of the PFZ code section. This macro tell the
 * linker that the storage for the variable here is at the start of the section */
extern char commpage_text_start[]
__SECTION_START_SYM(COMMPAGE_TEXT_SEGMENT, COMMPAGE_TEXT_SECTION);

/* Get a pointer to the end of the PFZ code section. This macro tell the
 * linker that the storage for the variable here is at the end of the section */
extern char commpage_text_end[]
__SECTION_END_SYM(COMMPAGE_TEXT_SEGMENT, COMMPAGE_TEXT_SECTION);

/* This is defined in the commpage text section as a symbol at the start of the preemptible
 * functions */
extern char commpage_text_preemptible_functions;

#if CONFIG_ARM_PFZ
static size_t size_of_pfz = 0;
#endif

/* This is the opcode for unimp, which traps as an illegal instruction */
#define UNIMP_OPCODE 0xC0001073

void
commpage_text_populate(void)
{
#if CONFIG_ARM_PFZ
	size_t size_of_commpage_text = commpage_text_end - commpage_text_start;
	if (size_of_commpage_text == 0) {
		panic("comm page text section %s,%s missing", COMMPAGE_TEXT_SEGMENT, COMMPAGE_TEXT_SECTION);
	}
	assert(size_of_commpage_text <= PAGE_SIZE);
	assert(size_of_commpage_text > 0);

	/* Get the size of the PFZ half of the comm page text section. */
	size_of_pfz = &commpage_text_preemptible_functions - commpage_text_start;

	// Copy the code segment of comm page text section into the PFZ
	memcpy((void *) _COMM_PAGE64_TEXT_START_ADDRESS, (void *) commpage_text_start, size_of_commpage_text);

	// fill the rest with unimp so a jump into it traps
	for (size_t off = (size_of_commpage_text + 3) & ~(size_t)3; off < PAGE_SIZE; off += sizeof(uint32_t)) {
		*(uint32_t *)((char *)_COMM_PAGE64_TEXT_START_ADDRESS + off) = UNIMP_OPCODE;
	}
#endif
}

uint32_t
commpage_is_in_pfz64(addr64_t addr64)
{
#if CONFIG_ARM_PFZ
	if ((addr64 >= commpage_text64_location) &&
	    (addr64 < (commpage_text64_location + size_of_pfz))) {
		return 1;
	} else {
		return 0;
	}
#else
#pragma unused (addr64)
	return 0;
#endif
}


void
commpage_set_timestamp(
	uint64_t        tbr,
	uint64_t        secs,
	uint64_t        frac,
	uint64_t        scale,
	uint64_t        tick_per_sec)
{
	new_commpage_timeofday_data_t *commpage_timeofday_datap;

	if (commPagePtr == 0) {
		return;
	}

	commpage_timeofday_datap =  (new_commpage_timeofday_data_t *)(_COMM_PAGE_NEWTIMEOFDAY_DATA + _COMM_PAGE_RW_OFFSET);

	commpage_timeofday_datap->TimeStamp_tick = 0x0ULL;

	__asm__ volatile ("fence rw, rw" ::: "memory");

	commpage_timeofday_datap->TimeStamp_sec = secs;
	commpage_timeofday_datap->TimeStamp_frac = frac;
	commpage_timeofday_datap->Ticks_scale = scale;
	commpage_timeofday_datap->Ticks_per_sec = tick_per_sec;

	__asm__ volatile ("fence rw, rw" ::: "memory");

	commpage_timeofday_datap->TimeStamp_tick = tbr;

}

// Update _COMM_PAGE_MEMORY_PRESSURE. Called periodically from vm's compute_memory_pressure()

void
commpage_set_memory_pressure(
	unsigned int    pressure )
{
	if (commPagePtr == 0) {
		return;
	}
	*((uint32_t *)(_COMM_PAGE_MEMORY_PRESSURE + _COMM_PAGE_RW_OFFSET)) = pressure;
}

// Determine number of CPUs on this system.
static int
commpage_cpus( void )
{
	int cpus;

	cpus = machine_info.max_cpus;

	if (cpus == 0) {
		panic("commpage cpus==0");
	}
	if (cpus > 0xFF) {
		cpus = 0xFF;
	}

	return cpus;
}

uint64_t
_get_cpu_capabilities(void)
{
	return _cpu_capabilities;
}

vm_address_t
_get_commpage_priv_address(void)
{
	return commpage_rw_addr;
}

vm_address_t
_get_commpage_ro_address(void)
{
	return commpage_kernel_ro_addr;
}

vm_address_t
_get_commpage_text_priv_address(void)
{
	return commpage_rw_text_addr;
}

// user visible isa extensions, as the device tree lists them for the boot hart
static void
commpage_init_riscv_optional_features(uint64_t *commpage_bits)
{
	static const struct {
		const char      *ext;
		uint64_t        bit;
	} features[] = {
		{ "f", kHasF },
		{ "d", kHasD },
		{ "v", kHasV },
		{ "zba", kHasZba },
		{ "zbb", kHasZbb },
		{ "zbs", kHasZbs },
		{ "zicbom", kHasZicbom },
		{ "zicboz", kHasZicboz },
		{ "zihintpause", kHasZihintpause },
		{ "zfh", kHasZfh },
		{ "zkt", kHasZkt },
		{ "zicond", kHasZicond },
		{ "zacas", kHasZacas },
		{ "svpbmt", kHasSvpbmt },
		{ "zawrs", kHasZawrs },
	};

	for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
		if (ml_riscv_isa_has(features[i].ext)) {
			*commpage_bits |= features[i].bit;
		}
	}
}

// Initialize _cpu_capabilities vector
static void
commpage_init_cpu_capabilities( void )
{
	uint64_t bits;
	int cpus;
	ml_cpu_info_t cpu_info;

	bits = 0;
	ml_cpu_get_info(&cpu_info);

	switch (cpu_info.cache_line_size) {
	case 128:
		bits |= kCache128;
		break;
	case 64:
		bits |= kCache64;
		break;
	case 32:
		bits |= kCache32;
		break;
	default:
		break;
	}
	cpus = commpage_cpus();

	if (cpus == 1) {
		bits |= kUP;
	}

	bits |= (cpus << kNumCPUsShift);

	bits |= kFastThreadLocalStorage;        // tp for TLS

	commpage_init_riscv_optional_features(&bits);

	_cpu_capabilities = bits;

	*((uint32_t *)(_COMM_PAGE_CPU_CAPABILITIES + _COMM_PAGE_RW_OFFSET)) = (uint32_t)_cpu_capabilities;
	*((uint64_t *)(_COMM_PAGE_CPU_CAPABILITIES64 + _COMM_PAGE_RW_OFFSET)) = _cpu_capabilities;

}

// Updated every time a logical CPU goes offline/online
void
commpage_update_active_cpus(void)
{
	if (!commPagePtr) {
		return;
	}
	*((uint8_t *)(_COMM_PAGE_ACTIVE_CPUS + _COMM_PAGE_RW_OFFSET)) = (uint8_t)processor_avail_count;

}

// Update the commpage bits for mach_absolute_time and mach_continuous_time (for userspace)
void
commpage_update_timebase(void)
{
	if (commPagePtr) {
		*((uint64_t*)(_COMM_PAGE_TIMEBASE_OFFSET + _COMM_PAGE_RW_OFFSET)) = rtclock_base_abstime;
	}
}

// Update the commpage with kdebug state (tracing enabled, typefilter present, continuous
// time for timestamps). Set to 0 if tracing is disabled.
void
commpage_update_kdebug_state(void)
{
	if (commPagePtr) {
		uint32_t state = kdebug_commpage_state();
		*((volatile uint32_t *)(_COMM_PAGE_KDEBUG_ENABLE + _COMM_PAGE_RW_OFFSET)) = state;
	}
}

/* Ditto for atm_diagnostic_config */
void
commpage_update_atm_diagnostic_config(uint32_t diagnostic_config)
{
	if (commPagePtr) {
		*((volatile uint32_t*)(_COMM_PAGE_ATM_DIAGNOSTIC_CONFIG + _COMM_PAGE_RW_OFFSET)) = diagnostic_config;
	}
}

// Update the commpage with this device's multiuser mode, so userspace services can
// avoid IPC in the common non-multiuser case.
void
commpage_update_multiuser_config(uint32_t multiuser_config)
{
	if (commPagePtr) {
		*((volatile uint32_t *)(_COMM_PAGE_MULTIUSER_CONFIG + _COMM_PAGE_RW_OFFSET)) = multiuser_config;
	}
}

// update the commpage data for last known value of mach_absolute_time()

void
commpage_update_mach_approximate_time(uint64_t abstime)
{
#ifdef CONFIG_MACH_APPROXIMATE_TIME
	if (!commPagePtr) {
		return;
	}

	uint64_t *approx_time_base = (uint64_t *)(uintptr_t)(_COMM_PAGE_APPROX_TIME + _COMM_PAGE_RW_OFFSET);

	uint64_t saved_data = os_atomic_load_wide(approx_time_base, relaxed);
	if (saved_data < abstime) {
		// ignore the cmpxchg result, if the value changed since we read it someone else
		// wrote a timestamp that is new enough.
		__unused bool ret = os_atomic_cmpxchg(approx_time_base,
		    saved_data, abstime, relaxed);
	}


#else /* CONFIG_MACH_APPROXIMATE_TIME */
#pragma unused (abstime)
#endif
}

// update the commpage data's total system sleep time for userspace mach_continuous_time()
void
commpage_update_mach_continuous_time(uint64_t sleeptime)
{
	if (!commPagePtr) {
		return;
	}

	uint64_t *cont_time_base = (uint64_t *)(uintptr_t)(_COMM_PAGE_CONT_TIMEBASE + _COMM_PAGE_RW_OFFSET);

	os_atomic_store_wide(cont_time_base, sleeptime, relaxed);

}

void
commpage_update_mach_continuous_time_hw_offset(uint64_t offset)
{
	*((uint64_t *)(_COMM_PAGE_CONT_HW_TIMEBASE + _COMM_PAGE_RW_OFFSET)) = offset;
}

// update the commpage's value for the boot time
void
commpage_update_boottime(uint64_t value)
{
	if (!commPagePtr) {
		return;
	}

	uint64_t *boottime_usec = (uint64_t *)(uintptr_t)(_COMM_PAGE_BOOTTIME_USEC + _COMM_PAGE_RW_OFFSET);

	os_atomic_store_wide(boottime_usec, value, relaxed);

}

// set the commpage's remote time params for userspace mach_bridge_remote_time()
void
commpage_set_remotetime_params(double rate, uint64_t base_local_ts, uint64_t base_remote_ts)
{
	if (commPagePtr) {
		struct bt_params *paramsp = (struct bt_params *)(_COMM_PAGE_REMOTETIME_PARAMS + _COMM_PAGE_RW_OFFSET);
		paramsp->base_local_ts = 0;
		__asm__ volatile ("fence rw, rw" ::: "memory");
		paramsp->rate = rate;
		paramsp->base_remote_ts = base_remote_ts;
		__asm__ volatile ("fence rw, rw" ::: "memory");
		paramsp->base_local_ts = base_local_ts;  //This will act as a generation count
	}
}


// update the commpage with if dtrace user land probes are enabled
void
commpage_update_dof(boolean_t enabled)
{
#if CONFIG_DTRACE
	*((uint8_t*)(_COMM_PAGE_DTRACE_DOF_ENABLED + _COMM_PAGE_RW_OFFSET)) = (enabled ? 1 : 0);
#else
	(void)enabled;
#endif
}

// update the dyld global config flags
void
commpage_update_dyld_flags(uint64_t value)
{
	*((uint64_t*)(_COMM_PAGE_DYLD_FLAGS + _COMM_PAGE_RW_OFFSET)) = value;

}

// update the APT active indicator
void
commpage_update_apt_active(bool active)
{
	uint8_t *slot = (uint8_t *)(void *)(_COMM_PAGE_APT_ACTIVE + _COMM_PAGE_RW_OFFSET);
	os_atomic_store(slot, active ? 1 : 0, relaxed);
}

// set the Extended Research Mode active indicator
void
commpage_set_erm_active(bool active)
{
	if (startup_phase < STARTUP_SUB_LOCKDOWN) {
		uint8_t *slot = (uint8_t *)(void *)(_COMM_PAGE_SECURITY_RESEARCH_DEVICE_ERM_ACTIVE + _COMM_PAGE_RW_OFFSET);
		os_atomic_store(slot, active ? 1 : 0, relaxed);
	}
#if DEVELOPMENT || DEBUG
	else {
		kprintf("ERROR can't set ERM bit at startup_phase 0x%x. Action is ignored\n", startup_phase);
	}
#endif
}
