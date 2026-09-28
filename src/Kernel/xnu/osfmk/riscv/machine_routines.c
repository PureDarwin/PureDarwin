/*
 * Copyright (c) 2007-2017 Apple Inc. All rights reserved.
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

#include <riscv/proc_reg.h>
#include <riscv/machine_cpu.h>
#include <riscv/cpu_internal.h>
#include <riscv/cpu_data.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/caches_internal.h>
#include <riscv/misc_protos.h>
#include <riscv/machine_routines.h>
#include <riscv/rtclock.h>
#include <riscv/cpu_capabilities.h>
#include <riscv/sbi.h>
#include <console/serial_protos.h>
#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <prng/random.h>
#include <kern/startup.h>
#include <kern/thread.h>
#include <kern/timer_queue.h>
#include <mach/machine.h>
#include <machine/atomic.h>
#include <machine/config.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_page_internal.h>
#include <vm/vm_pageout_xnu.h>
#include <vm/vm_shared_region_xnu.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_kern_xnu.h>
#include <sys/codesign.h>
#include <sys/kdebug.h>
#include <kern/coalition.h>
#include <pexpert/device_tree.h>
#include <pexpert/riscv/board_config.h>
#include <kern/smr.h>

#include <IOKit/IOPlatformExpert.h>
#if HIBERNATION
#include <IOKit/IOHibernatePrivate.h>
#endif /* HIBERNATION */

#include <libkern/OSAtomic.h>
#include <libkern/section_keywords.h>

MACHINE_TIMEOUT_DEV_WRITEABLE(LockTimeOut, "lock", 6e6 /* 0.25s */, MACHINE_TIMEOUT_UNIT_TIMEBASE, NULL);
machine_timeout_t LockTimeOutUsec; // computed in ml_init_lock_timeout

MACHINE_TIMEOUT_DEV_WRITEABLE(TLockTimeOut, "ticket-lock", 3e6 /* 0.125s */, MACHINE_TIMEOUT_UNIT_TIMEBASE, NULL);

TUNABLE_DEV_WRITEABLE(uint64_t, MutexSpin, "mutex-spin", 240 /* 10us */);

uint64_t low_MutexSpin;
int64_t high_MutexSpin;

static uint64_t ml_wfe_hint_max_interval;
#define MAX_WFE_HINT_INTERVAL_US (500ULL)

/* Must be less than cpu_idle_latency to ensure ml_delay_should_spin is true */
TUNABLE(uint32_t, yield_delay_us, "yield_delay_us", 0);

// the collection's extent, set up by riscv_vm_init
extern vm_offset_t segLOWESTKC, segHIGHESTKC;

// the hart the loader entered on, from riscv_init.c
extern uint32_t riscv_boot_hartid;

thread_t Idle_context(void);

SECURITY_READ_ONLY_LATE(bool) cpu_config_correct = true;
SECURITY_READ_ONLY_LATE(bool) cpu_config_modified = false;

SECURITY_READ_ONLY_LATE(static ml_topology_cpu_t) topology_cpu_array[MAX_CPUS];
SECURITY_READ_ONLY_LATE(static ml_topology_cluster_t) topology_cluster_array[MAX_CPU_CLUSTERS];
SECURITY_READ_ONLY_LATE(static ml_topology_info_t) topology_info = {
	.version = CPU_TOPOLOGY_VERSION,
	.cpus = topology_cpu_array,
	.clusters = topology_cluster_array,
};

_Atomic unsigned int cluster_type_num_active_cpus[MAX_CPU_TYPES];

// the boot hart's device tree node, its isa string describes every hart
SECURITY_READ_ONLY_LATE(static DTEntry) boot_cpu_entry = NULL;

// stimecmp lets s-mode arm its timer without a trip through sbi
SECURITY_READ_ONLY_LATE(static bool) riscv_has_sstc = false;

extern uint32_t lockdown_done;

// Regions of virtual address space reserved (pre-mapped) in each user address space.
static const struct vm_reserved_region vm_reserved_regions[] = {
	// Reserve the whole commpage nesting region so userspace can't allocate in it.
	// The commpage PTEs themselves are entered by vm_commpage_enter().
	{
		.vmrr_name = "commpage nesting",
		.vmrr_addr = _COMM_PAGE64_NESTING_START,
		.vmrr_size = _COMM_PAGE64_NESTING_SIZE
	}
};

// the absolute time the timer of each hart is armed for, in raw time csr ticks
static uint64_t riscv_timer_deadline[MAX_CPUS];

// every signal goes out through the sbi ipi extension, which has no deferral or retraction
void
ml_cpu_signal(unsigned int cpu_phys_id)
{
	long err = sbi_send_ipi(1UL, cpu_phys_id);

	if (__improbable(err != SBI_SUCCESS)) {
		panic("%s: sbi ipi to hart %u failed %ld", __func__, cpu_phys_id, err);
	}
}

void
ml_cpu_signal_deferred_adjust_timer(__unused uint64_t nanosecs)
{
}

uint64_t
ml_cpu_signal_deferred_get_timer()
{
	return 0;
}

// a deferred signal is only an early wake, sending it now keeps the same meaning
void
ml_cpu_signal_deferred(unsigned int cpu_phys_id)
{
	ml_cpu_signal(cpu_phys_id);
}

// an ipi already sent stays pending, the receiver finds no work and returns
void
ml_cpu_signal_retract(__unused unsigned int cpu_phys_id)
{
}

void
machine_idle(void)
{
	// Interrupts are expected to be masked on entry or re-entry via Idle_load_context()
	assert((csr_read(sstatus) & SSTATUS_SIE) == 0);
	Idle_context();
	csr_set(sstatus, SSTATUS_SIE);
}

__mockable boolean_t
ml_get_interrupts_enabled(void)
{
	return (csr_read(sstatus) & SSTATUS_SIE) ? TRUE : FALSE;
}

bool
ml_feature_supported(__unused uint64_t feature_bit)
{
	return false;
}

// Whether user mode may read the timebase as a continuous time source (mach_continuous_time).
boolean_t
user_cont_hwclock_allowed(void)
{
	return FALSE;
}

// Type of user mode timebase read.
// cpu_init opens the time csr to user mode through scounteren
uint8_t
user_timebase_type(void)
{
	return USER_TIMEBASE_SPEC;
}

void
machine_startup(__unused boot_args * args)
{
	machine_conf();

	// Kick off the kernel bootstrap.
	kernel_bootstrap();
	/* NOTREACHED */
}

bool
ml_is_secure_hib_supported(void)
{
	return false;
}

static void ml_release_deferred_pages(void);

void
machine_lockdown(void)
{
	riscv_vm_prot_finalize(PE_state.bootArgs);
	ml_release_deferred_pages();

	lockdown_done = 1;
}

char           *
machine_boot_info(
	__unused char *buf,
	__unused vm_size_t size)
{
	return PE_boot_args();
}

void
machine_cpu_reinit(__unused void *param)
{
	cpu_machine_init();     /* Initialize the processor */
	clock_init();           /* Init the clock */
}

thread_t
machine_processor_shutdown(
	__unused thread_t thread,
	void (*doshutdown)(processor_t),
	processor_t processor)
{
	return Shutdown_context(doshutdown, processor);
}

static void __startup_func
ml_init_lock_timeout(void)
{
	// Runs after STARTUP_SUB_TIMEOUTS, so these explicit legacy boot-args override the
	// ml-timeout-... config, which may come from the device tree.

	uint64_t lto_timeout_ns;
	uint64_t lto_abstime;
	uint32_t slto;

	if (PE_parse_boot_argn("slto_us", &slto, sizeof(slto))) {
		lto_timeout_ns = slto * NSEC_PER_USEC;
		nanoseconds_to_absolutetime(lto_timeout_ns, &lto_abstime);
		os_atomic_store(&LockTimeOut, lto_abstime, relaxed);
	} else {
		lto_abstime = os_atomic_load(&LockTimeOut, relaxed);
		absolutetime_to_nanoseconds(lto_abstime, &lto_timeout_ns);
	}

	os_atomic_store(&LockTimeOutUsec, lto_timeout_ns / NSEC_PER_USEC, relaxed);

	if (PE_parse_boot_argn("tlto_us", &slto, sizeof(slto))) {
		nanoseconds_to_absolutetime(slto * NSEC_PER_USEC, &lto_abstime);
		os_atomic_store(&TLockTimeOut, lto_abstime, relaxed);
	} else if (lto_abstime != 0) {
		os_atomic_store(&TLockTimeOut, lto_abstime >> 1, relaxed);
	} // else take default from MACHINE_TIMEOUT.

	uint64_t mtxspin;
	uint64_t mtx_abstime;
	if (PE_parse_boot_argn("mtxspin", &mtxspin, sizeof(mtxspin))) {
		if (mtxspin > USEC_PER_SEC >> 4) {
			mtxspin =  USEC_PER_SEC >> 4;
		}
		nanoseconds_to_absolutetime(mtxspin * NSEC_PER_USEC, &mtx_abstime);
		os_atomic_store(&MutexSpin, mtx_abstime, relaxed);
	} else {
		mtx_abstime = os_atomic_load(&MutexSpin, relaxed);
	}

	low_MutexSpin = os_atomic_load(&MutexSpin, relaxed);
	// high_MutexSpin should be low_MutexSpin * real_ncpus, but real_ncpus isn't set yet.
	// Active spinning is disabled on riscv, set high_MutexSpin via sysctl to enable it.
	high_MutexSpin = low_MutexSpin;

	uint64_t maxwfeus = MAX_WFE_HINT_INTERVAL_US;
	PE_parse_boot_argn("max_wfe_us", &maxwfeus, sizeof(maxwfeus));
	nanoseconds_to_absolutetime(maxwfeus * NSEC_PER_USEC, &ml_wfe_hint_max_interval);
}
STARTUP(TIMEOUTS, STARTUP_RANK_MIDDLE, ml_init_lock_timeout);


// Called once all ml_processor_info_t are initialized and all processors started via processor_boot().
// Required by the scheduler subsystem.
void
ml_cpu_init_completed(void)
{
	sched_cpu_init_completed();
}

// This tracks which cpus are between ml_cpu_down and ml_cpu_up
_Atomic uint64_t ml_cpu_up_processors = 0;

void
ml_cpu_up(void)
{
	cpu_data_t *cpu_data_ptr = getCpuDatap();

	assert(!bit_test(os_atomic_load(&ml_cpu_up_processors, relaxed), cpu_data_ptr->cpu_number));

	atomic_bit_set(&ml_cpu_up_processors, cpu_data_ptr->cpu_number, memory_order_relaxed);
}

// Machine-dependent info updates for the machine-independent cpu_up().
void
ml_cpu_up_update_counts(int cpu_id)
{
	ml_topology_cpu_t *cpu = &ml_get_topology_info()->cpus[cpu_id];

	os_atomic_inc(&cluster_type_num_active_cpus[cpu->cluster_type], relaxed);

	os_atomic_inc(&machine_info.physical_cpu, relaxed);
	os_atomic_inc(&machine_info.logical_cpu, relaxed);
}

int
ml_find_next_up_processor()
{
	if (BootCpuData.cpu_running) {
		return BootCpuData.cpu_number;
	}

	int next_active_cpu = lsb_first(os_atomic_load(&ml_cpu_up_processors, relaxed));

	if (next_active_cpu == -1) {
		assertf(ml_is_quiescing(), "can only have no active CPUs in quiesce state");
		next_active_cpu = BootCpuData.cpu_number;
	}

	return next_active_cpu;
}

// Machine-dependent info updates for the machine-independent cpu_down().
void
ml_cpu_down(void)
{
	// Handle outstanding IPIs early in processor_doshutdown, since decrementer interrupts
	// are pended via IPI when IRQ is masked.
	cpu_data_t *cpu_data_ptr = getCpuDatap();
	cpu_data_ptr->cpu_running = FALSE;

	assert((cpu_data_ptr->cpu_signal & SIGPdisabled) == 0);
	assert(bit_test(os_atomic_load(&ml_cpu_up_processors, relaxed), cpu_data_ptr->cpu_number));

	atomic_bit_clear(&ml_cpu_up_processors, cpu_data_ptr->cpu_number, memory_order_release);

	if (cpu_data_ptr == &BootCpuData && ml_is_quiescing()) {
		// Boot CPU powering down for S2R, no other active CPU to migrate its timers to.
		assert3u(os_atomic_load(&ml_cpu_up_processors, relaxed), ==, 0);
	} else if (cpu_data_ptr != &BootCpuData || (support_bootcpu_shutdown && !ml_is_quiescing())) {
		int next_cpu = ml_find_next_up_processor();

		cpu_data_t* new_cpu_datap = cpu_datap(next_cpu);

		// Move this cpu's timers to a cpu not in ml_cpu_down and poke it for a sooner deadline.
		// Relies on processor_updown_lock serializing ml_cpu_down.
		timer_queue_shutdown(next_cpu, &cpu_data_ptr->rtclock_timer.queue,
		    &new_cpu_datap->rtclock_timer.queue);

		// Run timer_queue_expire_local on the remote CPU. With interrupts disabled a cpu_xcall
		// would deadlock against the stackshot IPI, so this must be fire-and-forget.
		kern_return_t rv = cpu_signal(new_cpu_datap, SIGPTimerLocal, NULL, NULL);

		if (rv != KERN_SUCCESS) {
			panic("ml_cpu_down: cpu_signal of cpu %d failure %d", next_cpu, rv);
		}
	} else {
		panic("boot cpu powering down with nowhere for its timers to go");
	}

	cpu_signal_handler_internal(TRUE);

	/* There should be no more pending IPIs on this core. */
	assert3u(getCpuDatap()->cpu_signal, ==, SIGPdisabled);
}

void
ml_cpu_down_update_counts(int cpu_id)
{
	ml_topology_cpu_t *cpu = &ml_get_topology_info()->cpus[cpu_id];

	os_atomic_dec(&cluster_type_num_active_cpus[cpu->cluster_type], relaxed);

	os_atomic_dec(&machine_info.physical_cpu, relaxed);
	os_atomic_dec(&machine_info.logical_cpu, relaxed);
}


unsigned int
ml_get_machine_mem(void)
{
	return machine_info.memory_size;
}

__attribute__((noreturn))
void
halt_all_cpus(boolean_t reboot)
{
	if (reboot) {
		printf("MACH Reboot\n");
		PEHaltRestart(kPERestartCPU);
	} else {
		printf("CPU halted\n");
		PEHaltRestart(kPEHaltCPU);
	}

	// no platform driver took it, the firmware resets or powers off the whole system
	(void)sbi_system_reset(reboot ? SBI_SRST_TYPE_COLD_REBOOT : SBI_SRST_TYPE_SHUTDOWN, 0);

	(void)ml_set_interrupts_enabled(FALSE);
	while (1) {
		__asm__ volatile ("wfi");
	}
}

__attribute__((noreturn))
void
halt_cpu(void)
{
	halt_all_cpus(FALSE);
}

void
machine_signal_idle(
	processor_t processor)
{
	cpu_signal(processor_to_cpu_datap(processor), SIGPnop, (void *)NULL, (void *)NULL);
	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_AST), processor->cpu_id, 0 /* nop */);
}

void
machine_signal_idle_deferred(
	processor_t processor)
{
	cpu_signal_deferred(processor_to_cpu_datap(processor), SIGPdeferred);
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE, MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_DEFERRED_AST), processor->cpu_id, 0 /* nop */, 0, 0, 0);
}

void
machine_signal_idle_cancel(
	processor_t processor)
{
	cpu_signal_cancel(processor_to_cpu_datap(processor), SIGPdeferred);
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE, MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_CANCEL_AST), processor->cpu_id, 0 /* nop */, 0, 0, 0);
}

// Initialize Interrupt Handler
void
ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon)
{
	cpu_data_t     *cpu_data_ptr;
	boolean_t       current_state;

	current_state = ml_set_interrupts_enabled(FALSE);
	cpu_data_ptr = getCpuDatap();

	cpu_data_ptr->interrupt_nub = nub;
	cpu_data_ptr->interrupt_source = source;
	cpu_data_ptr->interrupt_target = target;
	cpu_data_ptr->interrupt_handler = handler;
	cpu_data_ptr->interrupt_refCon = refCon;
	csr_set(sie, SIE_SEIE);

	(void) ml_set_interrupts_enabled(current_state);
}

// Initialize Interrupts
void
ml_init_interrupt(void)
{
	// software interrupts carry ipis, external ones are enabled when the plic driver installs its handler
	csr_set(sie, SIE_SSIE);
}

// register and setup Timebase, Decrementer services
void
ml_init_timebase(
	void            *args,
	tbd_ops_t       tbd_funcs,
	__unused vm_offset_t     int_address,
	__unused vm_offset_t     int_value)
{
	cpu_data_t     *cpu_data_ptr;

	cpu_data_ptr = (cpu_data_t *)args;

	if ((cpu_data_ptr == &BootCpuData)
	    && (rtclock_timebase_func.tbd_fiq_handler == (void *)NULL)) {
		rtclock_timebase_func = *tbd_funcs;
	}
}

#define ML_READPROP_MANDATORY UINT64_MAX

static uint64_t
ml_readprop(const DTEntry entry, const char *propertyName, uint64_t default_value)
{
	void const *prop;
	unsigned int propSize;

	if (SecureDTGetProperty(entry, propertyName, &prop, &propSize) == kSuccess) {
		if (propSize == sizeof(uint8_t)) {
			return *((uint8_t const *)prop);
		} else if (propSize == sizeof(uint16_t)) {
			return *((uint16_t const *)prop);
		} else if (propSize == sizeof(uint32_t)) {
			return *((uint32_t const *)prop);
		} else if (propSize == sizeof(uint64_t)) {
			return *((uint64_t const *)prop);
		} else {
			panic("CPU property '%s' has bad size %u", propertyName, propSize);
		}
	} else {
		if (default_value == ML_READPROP_MANDATORY) {
			panic("Missing mandatory property '%s'", propertyName);
		}
		return default_value;
	}
}

static bool
ml_dt_string_is(const DTEntry entry, const char *propertyName, const char *value)
{
	void const *prop;
	unsigned int propSize;

	if (SecureDTGetProperty(entry, propertyName, &prop, &propSize) != kSuccess || propSize == 0) {
		return false;
	}
	return strncmp((char const *)prop, value, propSize) == 0;
}

// a missing status means the node is usable
static bool
ml_dt_is_enabled(const DTEntry entry)
{
	void const *prop;
	unsigned int propSize;

	if (SecureDTGetProperty(entry, "status", &prop, &propSize) != kSuccess) {
		return true;
	}
	return strncmp((char const *)prop, "okay", propSize) == 0 ||
	       strncmp((char const *)prop, "ok", propSize) == 0;
}

// cpu nodes share /cpus with cpu-map and cache nodes
static bool
ml_dt_is_cpu(const DTEntry entry)
{
	return ml_dt_string_is(entry, "device_type", "cpu") && ml_dt_is_enabled(entry);
}

// case blind compare of a length bounded token against a nul terminated name
static bool
ml_isa_token_is(const char *token, size_t len, const char *ext)
{
	size_t i;

	for (i = 0; i < len; i++) {
		char a = token[i], b = ext[i];

		if (b == '\0') {
			return false;
		}
		if (a >= 'A' && a <= 'Z') {
			a = (char)(a - 'A' + 'a');
		}
		if (a != b) {
			return false;
		}
	}
	return ext[i] == '\0';
}

static DTEntry
ml_riscv_isa_entry(void)
{
	OpaqueDTEntryIterator iter;
	DTEntry cpus, child;

	if (boot_cpu_entry != NULL) {
		return boot_cpu_entry;
	}
	if (SecureDTLookupEntry(NULL, "/cpus", &cpus) != kSuccess ||
	    SecureDTInitEntryIterator(cpus, &iter) != kSuccess) {
		return NULL;
	}
	while (SecureDTIterateEntries(&iter, &child) == kSuccess) {
		if (ml_dt_is_cpu(child)) {
			return child;
		}
	}
	return NULL;
}

// whether the boot hart has an isa extension, from riscv,isa-extensions or else the riscv,isa string
// single letters match the base letters, longer names the underscore separated tail
bool ml_riscv_isa_has(const char *ext);

bool
ml_riscv_isa_has(const char *ext)
{
	DTEntry entry = ml_riscv_isa_entry();
	char const *prop;
	unsigned int size;
	size_t extlen = strlen(ext);

	if (entry == NULL || extlen == 0) {
		return false;
	}

	if (SecureDTGetProperty(entry, "riscv,isa-extensions", (void const **)&prop, &size) == kSuccess) {
		for (unsigned int off = 0; off < size;) {
			size_t len = strnlen(prop + off, size - off);

			if (ml_isa_token_is(prop + off, len, ext)) {
				return true;
			}
			off += (unsigned int)len + 1;
		}
		return false;
	}

	if (SecureDTGetProperty(entry, "riscv,isa", (void const **)&prop, &size) != kSuccess) {
		return false;
	}

	size = (unsigned int)strnlen(prop, size);
	if (size < 4 || !ml_isa_token_is(prop, 4, "rv64")) {
		return false;
	}

	// the letters up to the first underscore, g stands for imafd plus zicsr and zifencei
	bool is_g_letter = extlen == 1 && (ext[0] == 'i' || ext[0] == 'm' || ext[0] == 'a' ||
	    ext[0] == 'f' || ext[0] == 'd');
	unsigned int off = 4;
	while (off < size && prop[off] != '_') {
		if (extlen == 1 && ml_isa_token_is(prop + off, 1, ext)) {
			return true;
		}
		if (is_g_letter && ml_isa_token_is(prop + off, 1, "g")) {
			return true;
		}
		off++;
	}

	while (off < size) {
		unsigned int start = ++off;

		while (off < size && prop[off] != '_') {
			off++;
		}
		if (ml_isa_token_is(prop + start, off - start, ext)) {
			return true;
		}
	}
	return false;
}

static void
ml_read_chip_revision(unsigned int *rev)
{
	struct sbiret ret = sbi_ecall(SBI_EXT_BASE, SBI_BASE_GET_MIMPID, 0, 0, 0, 0, 0, 0);

	*rev = (ret.error == SBI_SUCCESS) ? (unsigned int)ret.value : 0;
}

void
ml_parse_cpu_topology(void)
{
	DTEntry entry, child;
	OpaqueDTEntryIterator iter;
	uint32_t cpu_boot_arg = MAX_CPUS;
	uint64_t cpumask_boot_arg = ULLONG_MAX;
	int err;

	const boolean_t cpus_boot_arg_present = PE_parse_boot_argn("cpus", &cpu_boot_arg, sizeof(cpu_boot_arg));
	const boolean_t cpumask_boot_arg_present = PE_parse_boot_argn("cpumask", &cpumask_boot_arg, sizeof(cpumask_boot_arg));

	// The cpus=N and cpumask=N boot args cannot be used simultaneously. Flag this
	// so that we trigger a panic later in the boot process, once serial is enabled.
	if (cpus_boot_arg_present && cpumask_boot_arg_present) {
		cpu_config_correct = false;
	}

	/* The scheduler makes some assumptions at compile time that may not be true
	 * if cpus=N or cpumask=N boot-args are present. */
	if (cpus_boot_arg_present || cpumask_boot_arg_present) {
		cpu_config_modified = true;
	}

	err = SecureDTLookupEntry(NULL, "/cpus", &entry);
	assert(err == kSuccess);

	err = SecureDTInitEntryIterator(entry, &iter);
	assert(err == kSuccess);

	while (kSuccess == SecureDTIterateEntries(&iter, &child)) {
		if (!ml_dt_is_cpu(child)) {
			continue;
		}

		// the reg of a riscv cpu node is its hart id
		uint32_t hartid = (uint32_t)ml_readprop(child, "reg", ML_READPROP_MANDATORY);
		boolean_t is_boot_cpu = (hartid == riscv_boot_hartid);
		boolean_t cpu_enabled = cpumask_boot_arg & 1;
		cpumask_boot_arg >>= 1;

		// Boot CPU disabled in cpumask. Flag this so that we trigger a panic
		// later in the boot process, once serial is enabled.
		if (is_boot_cpu && !cpu_enabled) {
			cpu_config_correct = false;
		}

		// Ignore this CPU if it has been disabled by the cpumask= boot-arg.
		if (!is_boot_cpu && !cpu_enabled) {
			continue;
		}

		// With cpus= limiting CPUs and one slot left before the boot CPU is added,
		// skip non-boot CPUs to leave room for it.
		if (topology_info.num_cpus >= (cpu_boot_arg - 1) && topology_info.boot_cpu == NULL && !is_boot_cpu) {
			continue;
		}
		if (topology_info.num_cpus >= cpu_boot_arg || topology_info.num_cpus >= MAX_CPUS) {
			break;
		}

		ml_topology_cpu_t *cpu = &topology_info.cpus[topology_info.num_cpus];

		cpu->cpu_id = topology_info.num_cpus++;
		assert(cpu->cpu_id < MAX_CPUS);
		topology_info.max_cpu_id = MAX(topology_info.max_cpu_id, cpu->cpu_id);

		cpu->die_id = 0;
		cpu->phys_id = hartid;

		cpu->l2_cache_size = 0;
		cpu->l2_cache_id = 0;
		cpu->l3_cache_size = 0;
		cpu->l3_cache_id = 0;

		// every hart is the same kind and they form one cluster
		cpu->cluster_type = CLUSTER_TYPE_SMP;
		topology_info.cluster_type_num_cpus[cpu->cluster_type]++;
		cpu->cluster_id = 0;

		ml_topology_cluster_t *cluster = &topology_info.clusters[cpu->cluster_id];
		if (cluster->num_cpus == 0) {
			assert(topology_info.num_clusters < MAX_CPU_CLUSTERS);

			topology_info.num_clusters++;
			topology_info.max_cluster_id = MAX(topology_info.max_cluster_id, cpu->cluster_id);
			topology_info.cluster_types |= (1 << cpu->cluster_type);

			cluster->cluster_id = cpu->cluster_id;
			cluster->die_id = cpu->die_id;
			cluster->cluster_type = cpu->cluster_type;
			cluster->first_cpu_id = cpu->cpu_id;
			cluster->die_cluster_id = 0;

			topology_info.cluster_type_num_clusters[cluster->cluster_type]++;
		}

		cpu->die_cluster_id = 0;
		cpu->cluster_core_id = cluster->num_cpus;

		cpu->cpu_pset_id = PSET_ID_INVALID; /* initialized by ml_bootstrap_processors() */

		cluster->num_cpus++;
		cluster->cpu_mask |= 1ULL << cpu->cpu_id;

		if (is_boot_cpu) {
			assert(topology_info.boot_cpu == NULL);
			topology_info.boot_cpu = cpu;
			topology_info.boot_cluster = cluster;
			boot_cpu_entry = child;
		}
	}

	if (topology_info.boot_cpu == NULL) {
		panic("%s: boot hart %u has no enabled node under /cpus", __func__, riscv_boot_hartid);
	}
	ml_read_chip_revision(&topology_info.chip_revision);

	riscv_has_sstc = ml_riscv_isa_has("sstc");
}

const ml_topology_info_t *
ml_get_topology_info(void)
{
	return &topology_info;
}

// harts have no implementation defined register blocks to map
void
ml_map_cpu_pio(void)
{
}

__mockable unsigned int
ml_get_cpu_count(void)
{
	return topology_info.num_cpus;
}

unsigned int
ml_get_cluster_count(void)
{
	return topology_info.num_clusters;
}

int
ml_get_boot_cpu_number(void)
{
	return topology_info.boot_cpu->cpu_id;
}

cluster_type_t
ml_get_boot_cluster_type(void)
{
	return topology_info.boot_cluster->cluster_type;
}

int
ml_get_cpu_number(uint32_t phys_id)
{
	for (unsigned i = 0; i < topology_info.num_cpus; i++) {
		if (topology_info.cpus[i].phys_id == phys_id) {
			return i;
		}
	}

	return -1;
}

int
ml_get_cluster_number(uint32_t phys_id)
{
	int cpu_id = ml_get_cpu_number(phys_id);
	if (cpu_id < 0) {
		return -1;
	}

	ml_topology_cpu_t *cpu = &topology_info.cpus[cpu_id];

	return cpu->cluster_id;
}

// s-mode cannot read mhartid, gp holds this hart's cpu_data whenever the kernel runs
static inline cpu_data_t *
ml_local_cpu_data(void)
{
	cpu_data_t *cdp;

	__asm__ volatile ("mv %0, gp" : "=r"(cdp));
	return cdp;
}

unsigned int
ml_get_cpu_number_local(void)
{
	unsigned cpu_id;

	cpu_id = ml_local_cpu_data()->cpu_number;

	assert(cpu_id <= (unsigned int)ml_get_max_cpu_number());

	return cpu_id;
}

int
ml_get_cluster_number_local()
{
	unsigned cluster_id;

	cluster_id = ml_local_cpu_data()->cpu_cluster_id;

	assert(cluster_id <= (unsigned int)ml_get_max_cluster_number());

	return cluster_id;
}

int
ml_get_max_cpu_number(void)
{
	return topology_info.max_cpu_id;
}

int
ml_get_max_cluster_number(void)
{
	return topology_info.max_cluster_id;
}

unsigned int
ml_get_first_cpu_id(unsigned int cluster_id)
{
	return topology_info.clusters[cluster_id].first_cpu_id;
}

static_assert(MAX_CPUS <= 256, "MAX_CPUS must fit in _COMM_PAGE_CPU_TO_CLUSTER; Increase table size if needed");

void ml_map_cpus_to_clusters(uint8_t *table);

void
ml_map_cpus_to_clusters(uint8_t *table)
{
	for (uint16_t cpu_id = 0; cpu_id < topology_info.num_cpus; cpu_id++) {
		*(table + cpu_id) = (uint8_t)(topology_info.cpus[cpu_id].cluster_id);
	}
}

// Return the die id of a cluster.
unsigned int
ml_get_die_id(unsigned int cluster_id)
{
	unsigned int first_cpu = ml_get_first_cpu_id(cluster_id);
	return topology_info.cpus[first_cpu].die_id;
}

// Return the index of a cluster in its die.
unsigned int
ml_get_die_cluster_id(unsigned int cluster_id)
{
	unsigned int first_cpu = ml_get_first_cpu_id(cluster_id);
	return topology_info.cpus[first_cpu].die_cluster_id;
}

// Return the highest die id of the system.
unsigned int
ml_get_max_die_id(void)
{
	return topology_info.max_die_id;
}

void
ml_lockdown_init()
{
}

kern_return_t
ml_lockdown_handler_register(lockdown_handler_t f, void *this)
{
	if (!f) {
		return KERN_FAILURE;
	}

	assert(lockdown_done);
	f(this); // XXX: f this whole function

	return KERN_SUCCESS;
}

static mcache_flush_function mcache_flush_func;
static void* mcache_flush_service;
kern_return_t
ml_mcache_flush_callback_register(mcache_flush_function func, void *service)
{
	mcache_flush_service = service;
	mcache_flush_func = func;

	return KERN_SUCCESS;
}

kern_return_t
ml_mcache_flush(void)
{
	if (!mcache_flush_func) {
		panic("Cannot flush M$ with no flush callback registered");

		return KERN_FAILURE;
	} else {
		return mcache_flush_func(mcache_flush_service);
	}
}

kern_return_t ml_mem_fault_report_enable_register(void);
kern_return_t
ml_mem_fault_report_enable_register(void)
{
	return KERN_SUCCESS;
}

/* Initialize the percpu data and initialize processor structs. */
__startup_func
static void
ml_bootstrap_processors(void)
{
	assert(ml_get_interrupts_enabled() == false);
	for (unsigned cpu_id = 0; cpu_id < ml_get_cpu_count(); cpu_id++) {
		bool is_boot_cpu = (cpu_id == boot_cpu_id);
		cpu_data_t *this_cpu_datap;
		if (is_boot_cpu) {
			this_cpu_datap = &BootCpuData;
			/* initialized by riscv_init() */
		} else {
			this_cpu_datap = cpu_data_alloc(false);
			cpu_data_init(this_cpu_datap);
		}
		this_cpu_datap->cpu_number = (unsigned short)cpu_id;
		// the hart id is known now, ml_processor_register repeats it once iokit finds the cpu
		this_cpu_datap->cpu_phys_id = topology_info.cpus[cpu_id].phys_id;
		if (is_boot_cpu) {
			/* cpu_data_register()'ed by riscv_init(). */

			/* processor_init()'ed by processor_bootstrap(), but it skipped
			 * the SCHED(processor_init) callout. */
			SCHED(processor_init)(master_processor);
		} else {
			cpu_data_register(this_cpu_datap);

			/* Non-AMP platforms only support one pset. */
			processor_set_t pset = sched_boot_pset;
			assert3p(pset, !=, PROCESSOR_SET_NULL);
			processor_t processor = PERCPU_GET_RELATIVE(processor, cpu_data, this_cpu_datap);
			processor_init(processor, cpu_id, pset);
		}
		topology_info.cpus[cpu_id].cpu_pset_id = processor_array[cpu_id]->processor_set->pset_id;
	}
}
STARTUP(SCHED, STARTUP_RANK_SECOND, ml_bootstrap_processors);

kern_return_t
ml_processor_register(ml_processor_info_t *in_processor_info,
    processor_t *processor_out, ipi_handler_t *ipi_handler_out,
    perfmon_interrupt_handler_func *pmi_handler_out)
{
	cpu_data_t *this_cpu_datap = cpu_datap(in_processor_info->log_id);
	assert3u(this_cpu_datap->cpu_number, ==, in_processor_info->log_id); /* from ml_bootstrap_processors() */

	boolean_t  is_boot_cpu     = (in_processor_info->log_id == ml_get_boot_cpu_number());
	static unsigned int reg_cpu_count = 0;

	if (in_processor_info->log_id > (uint32_t)ml_get_max_cpu_number()) {
		return KERN_FAILURE;
	}

	if ((unsigned)OSIncrementAtomic((SInt32*)&reg_cpu_count) >= topology_info.num_cpus) {
		return KERN_FAILURE;
	}

	assert(in_processor_info->log_id <= (uint32_t)ml_get_max_cpu_number());

	this_cpu_datap->cpu_id = in_processor_info->cpu_id;

	this_cpu_datap->cpu_idle_notify = in_processor_info->processor_idle;
	this_cpu_datap->cpu_cache_dispatch = (cache_dispatch_t)in_processor_info->platform_cache_dispatch;
	nanoseconds_to_absolutetime((uint64_t) in_processor_info->powergate_latency, &this_cpu_datap->cpu_idle_latency);

	this_cpu_datap->idle_timer_notify = in_processor_info->idle_timer;
	this_cpu_datap->idle_timer_refcon = in_processor_info->idle_timer_refcon;

	this_cpu_datap->platform_error_handler = in_processor_info->platform_error_handler;
	this_cpu_datap->cpu_phys_id = in_processor_info->phys_id;

	this_cpu_datap->cpu_cluster_type = in_processor_info->cluster_type;
	this_cpu_datap->cpu_cluster_id = in_processor_info->cluster_id;
	this_cpu_datap->cpu_l2_id = in_processor_info->l2_cache_id;
	this_cpu_datap->cpu_l2_size = in_processor_info->l2_cache_size;
	this_cpu_datap->cpu_l3_id = in_processor_info->l3_cache_id;
	this_cpu_datap->cpu_l3_size = in_processor_info->l3_cache_size;

	this_cpu_datap->cluster_master = is_boot_cpu;

	processor_t processor = PERCPU_GET_WITH_BASE(other_percpu_base(this_cpu_datap->cpu_number), processor);
	*processor_out = processor;
	if (!is_boot_cpu) {
		smr_cpu_init(*processor_out);
	}
	*ipi_handler_out = cpu_signal_handler;
	*pmi_handler_out = NULL;
	if (in_processor_info->idle_tickle != (idle_tickle_t *) NULL) {
		*in_processor_info->idle_tickle = (idle_tickle_t) cpu_idle_tickle;
	}

	if (!is_boot_cpu) {
		random_cpu_init(this_cpu_datap->cpu_number);
		// now let next CPU register itself
		OSIncrementAtomic((SInt32*)&real_ncpus);
	}

	os_atomic_or(&this_cpu_datap->cpu_flags, InitState, relaxed);

	// The platform driver's processor_start is a no-op, so boot here. The boot processor needs
	// this to finish registerInterrupt and unblock the other cores.
	processor_boot(processor);

	return KERN_SUCCESS;
}

void
init_ast_check(
	__unused processor_t processor)
{
}

void
cause_ast_check(
	processor_t processor)
{
	assert(processor != PROCESSOR_NULL);

	if (current_processor() != processor) {
		cpu_signal(processor_to_cpu_datap(processor), SIGPast, (void *)NULL, (void *)NULL);
		KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_AST), processor->cpu_id, 1 /* ast */);
	}
}

void
cause_maintenance_ipi(int cpu)
{
	if (cpu != cpu_number()) {
		cpu_signal(CpuDataEntries[cpu].cpu_data_vaddr, SIGPMaintenance, NULL, NULL);
	}
}

extern uint32_t cpu_idle_count;

void
ml_get_power_state(boolean_t *icp, boolean_t *pidlep)
{
	*icp = ml_at_interrupt_context();
	*pidlep = (cpu_idle_count == real_ncpus);
}

// Generate a fake interrupt
void
ml_cause_interrupt(void)
{
	return;                 /* BS_XXX */
}

/* Map memory map IO space */
vm_offset_t
ml_io_map(
	vm_offset_t phys_addr,
	vm_size_t size)
{
	return io_map(phys_addr, size, VM_WIMG_IO, VM_PROT_DEFAULT, false);
}

/* Map memory map IO space (with protections specified) */
vm_offset_t
ml_io_map_with_prot(
	vm_offset_t phys_addr,
	vm_size_t size,
	vm_prot_t prot)
{
	return io_map(phys_addr, size, VM_WIMG_IO, prot, false);
}

vm_offset_t
ml_io_map_unmappable(
	vm_offset_t             phys_addr,
	vm_size_t               size,
	unsigned int            flags)
{
	return io_map(phys_addr, size, flags, VM_PROT_DEFAULT, true);
}

vm_offset_t
ml_io_map_wcomb(
	vm_offset_t phys_addr,
	vm_size_t size)
{
	return io_map(phys_addr, size, VM_WIMG_WCOMB, VM_PROT_DEFAULT, false);
}

void
ml_io_unmap(vm_offset_t addr, vm_size_t sz)
{
	pmap_remove(kernel_pmap, addr, addr + sz);
	kmem_free(kernel_map, addr, sz);
}

// sv39 has no fixed high window, a device mapping anywhere in the kernel map serves
vm_map_address_t
ml_map_high_window(
	vm_offset_t     phys_addr,
	vm_size_t       len)
{
	return io_map(phys_addr, len, VM_WIMG_IO, VM_PROT_READ | VM_PROT_WRITE, false);
}

vm_offset_t
ml_static_ptovirt(
	vm_offset_t paddr)
{
	return phystokv(paddr);
}

vm_offset_t
ml_static_slide(
	vm_offset_t vaddr)
{
	vm_offset_t slid_vaddr = vaddr + vm_kernel_slide;

	if (!VM_KERNEL_IS_SLID(slid_vaddr)) {
		/* This is only intended for use on static kernel addresses. */
		return 0;
	}

	return slid_vaddr;
}

vm_offset_t
ml_static_unslide(
	vm_offset_t vaddr)
{
	if (!VM_KERNEL_IS_SLID(vaddr)) {
		/* This is only intended for use on static kernel addresses. */
		return 0;
	}

	return vaddr - vm_kernel_slide;
}

// the leaf entry mapping a kernel va and its level, 1 for 1GB, 2 for 2MB and 3 for 4KB
// or NULL when nothing maps it
static pt_entry_t *
ml_riscv_kva_walk(vm_offset_t va, unsigned int *level)
{
	tt_entry_t *table = cpu_tte;
	const unsigned int shifts[] = { RISCV_TT_L1_SHIFT, RISCV_TT_L2_SHIFT, RISCV_TT_L3_SHIFT };

	for (unsigned int l = 0; l < 3; l++) {
		pt_entry_t *ptep = &table[(va >> shifts[l]) & (RISCV_TT_ENTRIES - 1)];
		pt_entry_t pte = *ptep;

		if (!(pte & PTE_V)) {
			return NULL;
		}
		if (pte & PTE_LEAF_MASK) {
			*level = l + 1;
			return ptep;
		}
		table = (tt_entry_t *)phystokv(PTE_TO_PA(pte));
	}
	return NULL;
}

pt_entry_t *ml_riscv_kva_to_pte(vm_offset_t va);

pt_entry_t *
ml_riscv_kva_to_pte(vm_offset_t va)
{
	unsigned int level;

	return ml_riscv_kva_walk(va, &level);
}

// kernel mappings are global, drop them from every hart's tlb
static void
ml_flush_kernel_tlb_region(vm_offset_t va, vm_size_t size)
{
	for (vm_offset_t cur = va; cur < va + size; cur += PAGE_SIZE) {
		sfence_vma_va(cur);
	}
	if (real_ncpus > 1) {
		(void)sbi_remote_sfence_vma(0, SBI_HART_MASK_BASE_ALL, va, size);
	}
}

kern_return_t
ml_static_protect(
	vm_offset_t vaddr, /* kernel virtual address */
	vm_size_t size,
	vm_prot_t new_prot)
{
	pt_entry_t    riscv_prot = PTE_R;
	vm_offset_t   vaddr_cur;
	ppnum_t       ppn;
	kern_return_t result = KERN_SUCCESS;

	if (vaddr < VM_MIN_KERNEL_ADDRESS) {
		panic("ml_static_protect(): %p < %p", (void *) vaddr, (void *) VM_MIN_KERNEL_ADDRESS);
		return KERN_FAILURE;
	}

	assert((vaddr & (PAGE_SIZE - 1)) == 0); /* must be page aligned */

	if ((new_prot & VM_PROT_WRITE) && (new_prot & VM_PROT_EXECUTE)) {
		panic("ml_static_protect(): WX request on %p", (void *) vaddr);
	}
	if (lockdown_done && (new_prot & VM_PROT_EXECUTE)) {
		panic("ml_static_protect(): attempt to inject executable mapping on %p", (void *) vaddr);
	}

	if (new_prot & VM_PROT_WRITE) {
		riscv_prot |= PTE_W;
	}
	if (new_prot & VM_PROT_EXECUTE) {
		riscv_prot |= PTE_X;
	}

	for (vaddr_cur = vaddr;
	    vaddr_cur < trunc_page_64(vaddr + size);
	    vaddr_cur += PAGE_SIZE) {
		ppn = pmap_find_phys(kernel_pmap, vaddr_cur);
		if (ppn != (vm_offset_t) NULL) {
			unsigned int level;
			pt_entry_t *pte_p = ml_riscv_kva_walk(vaddr_cur, &level);
			pt_entry_t ptmp;

			if (pte_p == NULL) {
				result = KERN_FAILURE;
				break;
			}

			ptmp = *pte_p;
			if (level != 3) {
				// ml_static_protect works on a block mapping that already has the
				// desired protections. Checks still run per page.
				if ((ptmp & PTE_LEAF_MASK) == riscv_prot) {
					continue;
				}

				result = KERN_FAILURE;
				break;
			}

			/* We only need to update the page tables if the protections do not match. */
			if ((ptmp & PTE_LEAF_MASK) != riscv_prot) {
				ptmp = (ptmp & ~PTE_LEAF_MASK) | riscv_prot | PTE_A;
				if (riscv_prot & PTE_W) {
					ptmp |= PTE_D;
				}
				*pte_p = ptmp;
			}
		}
	}

	if (vaddr_cur > vaddr) {
		__asm__ volatile ("fence rw, rw" ::: "memory");
		ml_flush_kernel_tlb_region(vaddr, vaddr_cur - vaddr);
	}

	return result;
}

// Pages ml_static_mfree()'d before lockdown, released all at once from machine_lockdown()
// after riscv_vm_prot_finalize() settles static region protections.
static
vm_page_list_t ml_static_mfree_pre_slide_list;

// Whether ml_static_mfree() still queues pages on ml_static_free_pre_slide_list
// instead of releasing them directly.
static
bool ml_static_mfree_queue_up = true;

// Release all pages queued up by ml_static_mfree() to the free queue.
static void
ml_release_deferred_pages(void)
{
	vm_page_free_list(ml_static_mfree_pre_slide_list.vmpl_head, false);
	ml_static_mfree_queue_up = false;
}

void
ml_static_mfree(
	vm_offset_t vaddr,
	vm_size_t   size)
{
	vm_offset_t vaddr_cur;
	vm_offset_t paddr_cur;
	ppnum_t     ppn;
	uint32_t    freed_pages = 0;
	uint32_t    freed_kernelcache_pages = 0;
	pmap_paddr_t kc_phys_start = kvtophys(segLOWESTKC);
	pmap_paddr_t kc_phys_end = kc_phys_start + (segHIGHESTKC - segLOWESTKC);

	/* It is acceptable (if bad) to fail to free. */
	if (vaddr < VM_MIN_KERNEL_ADDRESS) {
		return;
	}

	assert((vaddr & (PAGE_SIZE - 1)) == 0); /* must be page aligned */

	for (vaddr_cur = vaddr;
	    vaddr_cur < trunc_page_64(vaddr + size);
	    vaddr_cur += PAGE_SIZE) {
		// Some clients pass non-physical-aperture addresses, so convert to the physical aperture
		// address and drop all mappings while updating its protections.
		vm_offset_t vaddr_papt = phystokv(kvtophys(vaddr_cur));
		ppn = pmap_find_phys(kernel_pmap, vaddr_papt);

		if (ppn != (vm_offset_t) NULL) {
			// Failing to update protections on a page released to the VM is not acceptable.
			// Panic for now, to flag reclaimable memory.
			pmap_disconnect(ppn);
			if (ml_static_protect(vaddr_papt, PAGE_SIZE, VM_PROT_WRITE | VM_PROT_READ) != KERN_SUCCESS) {
				panic("Failed ml_static_mfree on %p", (void *) vaddr_cur);
			}

			paddr_cur = ptoa(ppn);

			if (__probable(!ml_static_mfree_queue_up)) {
				vm_page_create_canonical(ppn);
			} else {
				vm_page_t m = vm_page_create(ppn, true, Z_WAITOK);

				vm_page_list_push(&ml_static_mfree_pre_slide_list, m);
			}

			freed_pages++;
			if (paddr_cur >= kc_phys_start && paddr_cur < kc_phys_end) {
				freed_kernelcache_pages++;
			}
		}
	}

	vm_page_lockspin_queues();
	vm_page_wire_count -= freed_pages;
	vm_page_wire_count_initial -= freed_pages;
	vm_page_kernelcache_count -= freed_kernelcache_pages;
	vm_page_unlock_queues();
#if DEBUG
	kprintf("%s: Released %u pages at VA %p, size: %llu, last ppn: %#x, %u from kernelcache\n",
	    __func__, freed_pages, (void *)vaddr, (uint64_t)size, ppn, freed_kernelcache_pages);
#endif
}

// Returns the type of page protection that the system supports.
ml_page_protection_t
ml_page_protection_type(void)
{
	return 0;
}

// Whether the device is production-fused.
// no riscv part carries such a fuse, report fused as the i386 version does
boolean_t
ml_device_is_prod_fused(void)
{
	return 1;
}

/* virtual to physical on wired pages */
vm_offset_t
ml_vtophys(vm_offset_t vaddr)
{
	return kvtophys(vaddr);
}

// ml_nofault_copy and ml_validate_nofault live with the copy routines in copyio.c

void
ml_get_bouncepool_info(vm_offset_t * phys_addr, vm_size_t * size)
{
	*phys_addr = 0;
	*size = 0;
}

void
active_rt_threads(__unused boolean_t active)
{
}

static void
cpu_qos_cb_default(__unused int urgency, __unused uint64_t qos_param1, __unused uint64_t qos_param2)
{
	return;
}

cpu_qos_update_t cpu_qos_update = cpu_qos_cb_default;

void
cpu_qos_update_register(cpu_qos_update_t cpu_qos_cb)
{
	if (cpu_qos_cb != NULL) {
		cpu_qos_update = cpu_qos_cb;
	} else {
		cpu_qos_update = cpu_qos_cb_default;
	}
}

void
thread_tell_urgency(thread_urgency_t urgency, uint64_t rt_period, uint64_t rt_deadline, uint64_t sched_latency __unused, __unused thread_t nthread)
{
	SCHED_DEBUG_PLATFORM_KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_SCHED, MACH_URGENCY) | DBG_FUNC_START, urgency, rt_period, rt_deadline, sched_latency, 0);

	cpu_qos_update((int)urgency, rt_period, rt_deadline);

	SCHED_DEBUG_PLATFORM_KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_SCHED, MACH_URGENCY) | DBG_FUNC_END, urgency, rt_period, rt_deadline, 0, 0);
}

void
machine_run_count(__unused uint32_t count)
{
}

#if KASAN
vm_offset_t ml_stack_base(void);
vm_size_t ml_stack_size(void);

vm_offset_t
ml_stack_base(void)
{
	uintptr_t local = (uintptr_t) &local;
	vm_offset_t     intstack_top_ptr;

	intstack_top_ptr = getCpuDatap()->intstack_top;
	if ((local < intstack_top_ptr) && (local > intstack_top_ptr - INTSTACK_SIZE)) {
		return intstack_top_ptr - INTSTACK_SIZE;
	} else {
		return current_thread()->kernel_stack;
	}
}
vm_size_t
ml_stack_size(void)
{
	uintptr_t local = (uintptr_t) &local;
	vm_offset_t     intstack_top_ptr;

	intstack_top_ptr = getCpuDatap()->intstack_top;
	if ((local < intstack_top_ptr) && (local > intstack_top_ptr - INTSTACK_SIZE)) {
		return INTSTACK_SIZE;
	} else {
		return kernel_stack_size;
	}
}
#endif

#ifdef CONFIG_KCOV

kcov_cpu_data_t *
current_kcov_data(void)
{
	return &current_cpu_datap()->cpu_kcov_data;
}

kcov_cpu_data_t *
cpu_kcov_data(int cpuid)
{
	return &cpu_datap(cpuid)->cpu_kcov_data;
}

#endif /* CONFIG_KCOV */

boolean_t
machine_timeout_suspended(void)
{
	return FALSE;
}

kern_return_t
ml_interrupt_prewarm(__unused uint64_t deadline)
{
	return KERN_FAILURE;
}

#define CSR_STIMECMP    0x14d

// Assumes interrupts disabled.
// the decrementer counts ticks from now, the timer compares against an absolute time
void
ml_set_decrementer(uint32_t dec_value)
{
	cpu_data_t      *cdp = getCpuDatap();

	assert(ml_get_interrupts_enabled() == FALSE);
	cdp->cpu_decrementer = dec_value;

	if (cdp->cpu_set_decrementer_func) {
		cdp->cpu_set_decrementer_func(dec_value);
	} else {
		uint64_t deadline = ml_get_hwclock() + dec_value;

		riscv_timer_deadline[cdp->cpu_number] = deadline;
		if (riscv_has_sstc) {
			__asm__ volatile ("csrw %0, %1" :: "i"(CSR_STIMECMP), "r"(deadline) : "memory");
		} else {
			sbi_set_timer(deadline);
		}
	}
	csr_set(sie, SIE_STIE);
}

static inline uint64_t
speculative_timebase(void)
{
	uint64_t t;

	__asm__ volatile ("rdtime %0" : "=r"(t));
	return t;
}

// a fence keeps earlier memory accesses ahead of the read
static inline uint64_t
nonspeculative_timebase(void)
{
	__asm__ volatile ("fence iorw, iorw" ::: "memory");
	return speculative_timebase();
}

uint64_t
ml_get_hwclock()
{
	uint64_t timebase = nonspeculative_timebase();
	return timebase;
}

uint64_t
ml_get_hwclock_speculative()
{
	uint64_t timebase = speculative_timebase();
	return timebase;
}

uint64_t
ml_get_timebase()
{
	uint64_t clock, timebase;

	//the retry is for the case where S2R catches us in the middle of this. see rdar://77019633
	do {
		timebase = getCpuDatap()->cpu_base_timebase;
		os_compiler_barrier();
		clock = ml_get_hwclock();
		os_compiler_barrier();
	} while (getCpuDatap()->cpu_base_timebase != timebase);

	return clock + timebase;
}

// Barrier ensuring all prior memory accesses complete before any subsequent timebase reads.
void
ml_memory_to_timebase_fence(void)
{
	__asm__ volatile ("fence iorw, iorw" ::: "memory");

	/* throwaway read to prevent ml_get_speculative_timebase() reordering */
	(void)ml_get_hwclock();
}

// Barrier ordering all prior timebase reads before any subsequent memory accesses.
void
ml_timebase_to_memory_fence(void)
{
	__asm__ volatile ("fence iorw, iorw" ::: "memory");
}

// Get the speculative timebase without a fence.
uint64_t
ml_get_speculative_timebase(void)
{
	uint64_t clock, timebase;

	//the retry is for the case where S2R catches us in the middle of this. see rdar://77019633&77697482
	do {
		timebase = getCpuDatap()->cpu_base_timebase;
		os_compiler_barrier();
		clock = speculative_timebase();

		os_compiler_barrier();
	} while (getCpuDatap()->cpu_base_timebase != timebase);

	return clock + timebase;
}

uint64_t
ml_get_timebase_entropy(void)
{
	return ml_get_speculative_timebase();
}

uint32_t
ml_get_decrementer(void)
{
	cpu_data_t *cdp = getCpuDatap();
	uint32_t dec;

	assert(ml_get_interrupts_enabled() == FALSE);

	if (cdp->cpu_get_decrementer_func) {
		dec = cdp->cpu_get_decrementer_func();
	} else {
		uint64_t now = ml_get_hwclock();
		uint64_t deadline = riscv_timer_deadline[cdp->cpu_number];

		dec = (deadline > now) ? (uint32_t)MIN(deadline - now, UINT32_MAX) : 0;
	}

	return dec;
}

boolean_t
ml_get_timer_pending(void)
{
	// sip.stip sits at the same bit as sie.stie
	return (csr_read(sip) & SIE_STIE) ? TRUE : FALSE;
}

// the user tp as the thread records it, machine_thread_set_tsd_base updates both
static vm_address_t
riscv_get_cthread_self(void)
{
	return current_thread()->machine.cthread_self;
}

__attribute__((noreturn))
void
platform_syscall(riscv_saved_state_t *state)
{
	uint32_t code;

#define platform_syscall_kprintf(x...) /* kprintf("platform_syscall: " x) */

	// t0 selects the platform trap itself, the call code rides in a3 as arm keeps it in x3
	code = (uint32_t)get_saved_state_reg(state, RISCV_REG_A0 + 3);

	KDBG(MACHDBG_CODE(DBG_MACH_MACHDEP_EXCP_SC_ARM, code) | DBG_FUNC_START,
	    get_saved_state_reg(state, RISCV_REG_A0),
	    get_saved_state_reg(state, RISCV_REG_A0 + 1),
	    get_saved_state_reg(state, RISCV_REG_A0 + 2));

	switch (code) {
	case 2:
		/* set cthread */
		platform_syscall_kprintf("set cthread self.\n");
		machine_thread_set_tsd_base(current_thread(), get_saved_state_reg(state, RISCV_REG_A0));
		break;
	case 3:
		/* get cthread */
		platform_syscall_kprintf("get cthread self.\n");
		set_saved_state_reg(state, RISCV_REG_A0, riscv_get_cthread_self());
		break;
	case 0:
		// fence.i only reaches the calling hart, code written by userland must reach every hart
		platform_syscall_kprintf("icache flush.\n");
		invalidate_icache64(0, 0, FALSE);
		break;
	case 1: /* D-Cache flush (removed) */
	default:
		platform_syscall_kprintf("unknown: %d\n", code);
		break;
	}

	KDBG(MACHDBG_CODE(DBG_MACH_MACHDEP_EXCP_SC_ARM, code) | DBG_FUNC_END,
	    get_saved_state_reg(state, RISCV_REG_A0));

	thread_exception_return();
}

boolean_t
ml_delay_should_spin(uint64_t interval)
{
	cpu_data_t     *cdp = getCpuDatap();

	if (cdp->cpu_idle_latency) {
		return (interval < cdp->cpu_idle_latency) ? TRUE : FALSE;
	} else {
		// Early boot, latency is unknown. Blocking is always safe, even if slow
		return FALSE;
	}
}

boolean_t
ml_thread_is64bit(thread_t thread)
{
	return thread_is_64bit_addr(thread);
}

void
ml_delay_on_yield(void)
{
#if DEVELOPMENT || DEBUG
	if (yield_delay_us) {
		delay(yield_delay_us);
	}
#endif
}

void
ml_timer_evaluate(void)
{
}

boolean_t
ml_timer_forced_evaluation(void)
{
	return FALSE;
}

void
ml_gpu_stat_update(__unused uint64_t gpu_ns_delta)
{
	// For now: update the resource coalition stats of the current thread's coalition
	task_coalition_update_gpu_stats(current_task(), gpu_ns_delta);
}

uint64_t
ml_gpu_stat(__unused thread_t t)
{
	return 0;
}

thread_t
current_thread(void)
{
	return current_thread_fast();
}

#if DEVELOPMENT || DEBUG
static uint64_t minor_badness_suffered = 0;
#endif
void
ml_report_minor_badness(uint32_t __unused badness_id)
{
	#if DEVELOPMENT || DEBUG
	(void)os_atomic_or(&minor_badness_suffered, 1ULL << badness_id, relaxed);
	#endif
}

void
ml_hibernate_active_pre(void)
{
#if HIBERNATION
	if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
		hibernate_rebuild_vm_structs();
	}
#endif /* HIBERNATION */
}

void
ml_hibernate_active_post(void)
{
#if HIBERNATION
	if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
		hibernate_machine_init();
		hibernate_vm_lock_end();
		current_cpu_datap()->cpu_hibernate = 0;
	}
#endif /* HIBERNATION */
}

// Returns the machine-dependent regions the VM reserves (pre-maps) so user processes
// can't allocate or deallocate in them, count returned, array through `regions`.
size_t
ml_get_vm_reserved_regions(bool vm_is64bit, const struct vm_reserved_region **regions)
{
	assert(regions != NULL);

	if (vm_is64bit) {
		*regions = vm_reserved_regions;
		return ARRAY_COUNT(vm_reserved_regions);
	} else {
		*regions = NULL;
		return 0;
	}
}

// WFE recommendations update infrequently, possibly from another cluster,
// so false cacheline sharing isn't material.
static uint64_t riscv_cluster_wfe_recs[MAX_CPU_CLUSTERS];

uint32_t
ml_update_cluster_wfe_recommendation(uint32_t wfe_cluster_id, uint64_t wfe_timeout_abstime_interval, __unused uint64_t wfe_hint_flags)
{
	assert(wfe_cluster_id < MAX_CPU_CLUSTERS);
	assert(wfe_timeout_abstime_interval <= ml_wfe_hint_max_interval);
	os_atomic_store(&riscv_cluster_wfe_recs[wfe_cluster_id], wfe_timeout_abstime_interval, relaxed);
	return 0; /* Success */
}

uint64_t ml_cluster_wfe_timeout(uint32_t wfe_cluster_id);

uint64_t
ml_cluster_wfe_timeout(uint32_t wfe_cluster_id)
{
	// This and its consumer don't synchronize with recommendation updates, races are acceptable.
	return os_atomic_load(&riscv_cluster_wfe_recs[wfe_cluster_id], relaxed);
}

__pure2 bool
ml_addr_in_non_xnu_stack(__unused uintptr_t addr)
{
	return false;
}

uint64_t
ml_get_backtrace_pc(struct riscv_saved_state *state)
{
	assert(state != NULL);

	return get_saved_state_pc(state);
}

// Preallocates a floating point save area. A noop since preallocation isn't required.
void
ml_fp_save_area_prealloc(void)
{
}

void
ml_task_post_signature_processing_hook(__unused task_t task)
{
	// Acquire barrier so the machine flags read below isn't speculated before the
	// task->t_returnwaitflags read in task_wait_to_return().
	os_atomic_thread_fence(acquire);
}

// no monitor sits between the kernel and the firmware, so nothing needs unwinding before a panic
void
ml_panic_trap_to_debugger(__unused const char *panic_format_str,
    __unused va_list *panic_args,
    __unused unsigned int reason,
    __unused void *ctx,
    __unused uint64_t panic_options_mask,
    __unused unsigned long panic_caller,
    __unused const char *panic_initiator)
{
}

bool ml_unsafe_kernel_text(void);

bool
ml_unsafe_kernel_text(void)
{
	/* Kernel text is never writable under these configs. */
	return false;
}
