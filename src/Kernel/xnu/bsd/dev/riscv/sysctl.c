/*
 * Copyright (c) 2003-2007 Apple Inc. All rights reserved.
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/machine_routines.h>

#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/machine/vm_param.h>
#include <vm/pmap.h>
#include <kern/zalloc.h>
#include <libkern/libkern.h>
#include <pexpert/device_tree.h>
#include <kern/task.h>
#include <vm/vm_protos.h>

#define __STR(x)        #x
#define STRINGIFY(x)    __STR(x)

extern uint64_t wake_abstime;

static
SYSCTL_QUAD(_machdep, OID_AUTO, wake_abstime,
    CTLFLAG_RD, &wake_abstime,
    "Absolute Time at the last wakeup");

static int
sysctl_time_since_reset SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	uint64_t return_value = ml_get_time_since_reset();
	return SYSCTL_OUT(req, &return_value, sizeof(return_value));
}

SYSCTL_PROC(_machdep, OID_AUTO, time_since_reset,
    CTLFLAG_RD | CTLTYPE_QUAD | CTLFLAG_LOCKED,
    0, 0, sysctl_time_since_reset, "I",
    "Continuous time since last SOC boot/wake started");

static int
sysctl_wake_conttime SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	uint64_t return_value = ml_get_conttime_wake_time();
	return SYSCTL_OUT(req, &return_value, sizeof(return_value));
}

SYSCTL_PROC(_machdep, OID_AUTO, wake_conttime,
    CTLFLAG_RD | CTLTYPE_QUAD | CTLFLAG_LOCKED,
    0, 0, sysctl_wake_conttime, "I",
    "Continuous Time at the last wakeup");

#if defined(HAS_IPI)
static int
cpu_signal_deferred_timer(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	int new_value = 0;
	int changed   = 0;

	int old_value = (int)ml_cpu_signal_deferred_get_timer();

	int error = sysctl_io_number(req, old_value, sizeof(int), &new_value, &changed);

	if (error == 0 && changed) {
		ml_cpu_signal_deferred_adjust_timer((uint64_t)new_value);
	}

	return error;
}

SYSCTL_PROC(_machdep, OID_AUTO, deferred_ipi_timeout,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0,
    cpu_signal_deferred_timer, "I", "Deferred IPI timeout (nanoseconds)");

#endif /* defined(HAS_IPI) */

// For source compatibility, machdep.cpu mibs that use host_info() to simulate
// reasonable answers.

SYSCTL_NODE(_machdep, OID_AUTO, cpu, CTLFLAG_RW | CTLFLAG_LOCKED, 0,
    "CPU info");

static int
riscv_host_info SYSCTL_HANDLER_ARGS
{
	__unused struct sysctl_oid *unused_oidp = oidp;

	host_basic_info_data_t hinfo;
	mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
#define BSD_HOST        1
	kern_return_t kret = host_info((host_t)BSD_HOST,
	    HOST_BASIC_INFO, (host_info_t)&hinfo, &count);
	if (KERN_SUCCESS != kret) {
		return EINVAL;
	}

	if (sizeof(uint32_t) != arg2) {
		panic("size mismatch");
	}

	uintptr_t woffset = (uintptr_t)arg1 / sizeof(uint32_t);
	uint32_t datum = *(uint32_t *)(((uint32_t *)&hinfo) + woffset);
	return SYSCTL_OUT(req, &datum, sizeof(datum));
}

// machdep.cpu.cores_per_package: physical harts, aka hw.physicalcpu_max.
static
SYSCTL_PROC(_machdep_cpu, OID_AUTO, cores_per_package,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    (void *)offsetof(host_basic_info_data_t, physical_cpu_max),
    sizeof(integer_t),
    riscv_host_info, "I", "CPU cores per package");

// machdep.cpu.core_count: active physical harts, aka hw.physicalcpu.
static
SYSCTL_PROC(_machdep_cpu, OID_AUTO, core_count,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    (void *)offsetof(host_basic_info_data_t, physical_cpu),
    sizeof(integer_t),
    riscv_host_info, "I", "Number of enabled cores per package");

// machdep.cpu.logical_per_package: total logical harts, aka hw.logicalcpu_max.
static
SYSCTL_PROC(_machdep_cpu, OID_AUTO, logical_per_package,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    (void *)offsetof(host_basic_info_data_t, logical_cpu_max),
    sizeof(integer_t),
    riscv_host_info, "I", "CPU logical cpus per package");

// machdep.cpu.thread_count: active logical harts, aka hw.logicalcpu.
static
SYSCTL_PROC(_machdep_cpu, OID_AUTO, thread_count,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    (void *)offsetof(host_basic_info_data_t, logical_cpu),
    sizeof(integer_t),
    riscv_host_info, "I", "Number of enabled threads per package");

static SECURITY_READ_ONLY_LATE(char*) brand_string = NULL;
static SECURITY_READ_ONLY_LATE(size_t) brand_string_len = 0;

// the product node is what arm boots have, a plain fdt boot only has the root compatible
static bool
sysctl_brand_string_prop(const char *path, const char *prop)
{
	DTEntry node;
	void const *value = NULL;
	unsigned int size = 0;

	if (kSuccess != SecureDTLookupEntry(0, path, &node)) {
		return false;
	}

	if (kSuccess != SecureDTGetProperty(node, prop, (void const **) &value, &size)) {
		return false;
	}

	if (size == 0) {
		return false;
	}

	// a compatible list is several strings, keep the first one
	size = (unsigned int)strnlen(value, size) + 1;

	brand_string = zalloc_permanent(size, ZALIGN_NONE);
	if (brand_string == NULL) {
		return false;
	}

	memcpy(brand_string, value, size);
	brand_string[size - 1] = '\0';
	brand_string_len = size;
	return true;
}

// SecureDTLookupEntry() only works before PE_init_iokit(), so load the brand string
// (if available) in a startup handler.
__startup_func
static void
sysctl_load_brand_string(void)
{
	if (!sysctl_brand_string_prop("/product", "product-soc-name")) {
		sysctl_brand_string_prop("/", "compatible");
	}
}
STARTUP(SYSCTL, STARTUP_RANK_MIDDLE, sysctl_load_brand_string);

// machdep.cpu.brand_string: product string from the device tree, else a generic name.
static int
make_brand_string SYSCTL_HANDLER_ARGS
{
	__unused struct sysctl_oid *unused_oidp = oidp;
	__unused void *unused_arg1 = arg1;
	__unused int unused_arg2 = arg2;

	if (brand_string != NULL) {
		return SYSCTL_OUT(req, brand_string, brand_string_len);
	}

	const char *buf = "RISC-V processor";
	return SYSCTL_OUT(req, buf, strlen(buf) + 1);
}

SYSCTL_PROC(_machdep_cpu, OID_AUTO, brand_string,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, make_brand_string, "A", "CPU brand string");


static int
virtual_address_size SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	// the user half of sv39
	int return_value = 64 - __builtin_clzll(MACH_VM_MAX_ADDRESS_RAW - 1);
	return SYSCTL_OUT(req, &return_value, sizeof(return_value));
}

static
SYSCTL_PROC(_machdep, OID_AUTO, virtual_address_size,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, virtual_address_size, "I",
    "Number of addressable bits in userspace virtual addresses");


#if DEVELOPMENT || DEBUG
// generates a sysctl machdep.cpu.sysreg_* for a given supervisor csr.
#define SYSCTL_PROC_MACHDEP_CPU_SYSREG(name)                            \
static int                                                              \
sysctl_sysreg_##name SYSCTL_HANDLER_ARGS                                \
{                                                                       \
_Pragma("unused(arg1, arg2, oidp)")                                     \
	uint64_t return_value;                                              \
	__asm__ volatile ("csrr %0, " #name : "=r"(return_value));          \
	return SYSCTL_OUT(req, &return_value, sizeof(return_value));        \
}                                                                       \
SYSCTL_PROC(_machdep_cpu, OID_AUTO, sysreg_##name,                      \
    CTLFLAG_RD | CTLTYPE_QUAD | CTLFLAG_LOCKED,                         \
    0, 0, sysctl_sysreg_##name, "Q",                                    \
    #name " register on the current CPU");


// supervisor trap vector base
SYSCTL_PROC_MACHDEP_CPU_SYSREG(stvec);
// supervisor address translation and protection
SYSCTL_PROC_MACHDEP_CPU_SYSREG(satp);
// supervisor status
SYSCTL_PROC_MACHDEP_CPU_SYSREG(sstatus);
// supervisor interrupt enable
SYSCTL_PROC_MACHDEP_CPU_SYSREG(sie);

#endif /* DEVELOPMENT || DEBUG */


#ifdef ML_IO_TIMEOUTS_ENABLED
// Timeouts for ml_{io|phys}_{read|write}, RO on DEVELOPMENT/DEBUG kernels.

#if DEVELOPMENT || DEBUG
#define MMIO_TIMEOUT_FLAGS (CTLFLAG_KERN | CTLFLAG_RW | CTLFLAG_LOCKED)
#else
#define MMIO_TIMEOUT_FLAGS (CTLFLAG_KERN | CTLFLAG_RD | CTLFLAG_LOCKED)
#endif

SYSCTL_QUAD(_machdep, OID_AUTO, report_phy_read_delay, MMIO_TIMEOUT_FLAGS,
    &report_phy_read_delay_to, "Maximum time before io/phys read gets reported or panics");
SYSCTL_QUAD(_machdep, OID_AUTO, report_phy_write_delay, MMIO_TIMEOUT_FLAGS,
    &report_phy_write_delay_to, "Maximum time before io/phys write gets reported or panics");
SYSCTL_QUAD(_machdep, OID_AUTO, trace_phy_read_delay, MMIO_TIMEOUT_FLAGS,
    &trace_phy_read_delay_to, "Maximum time before io/phys read gets ktraced");
SYSCTL_QUAD(_machdep, OID_AUTO, trace_phy_write_delay, MMIO_TIMEOUT_FLAGS,
    &trace_phy_write_delay_to, "Maximum time before io/phys write gets ktraced");

SYSCTL_INT(_machdep, OID_AUTO, phy_read_delay_panic, CTLFLAG_KERN | CTLFLAG_RW | CTLFLAG_LOCKED,
    &phy_read_panic, 0, "if set, report-phy-read-delay timeout panics");
SYSCTL_INT(_machdep, OID_AUTO, phy_write_delay_panic, CTLFLAG_KERN | CTLFLAG_RW | CTLFLAG_LOCKED,
    &phy_write_panic, 0, "if set, report-phy-write-delay timeout panics");

#if ML_IO_SIMULATE_STRETCHED_ENABLED
SYSCTL_QUAD(_machdep, OID_AUTO, sim_stretched_io_ns, CTLFLAG_KERN | CTLFLAG_RW | CTLFLAG_LOCKED,
    &simulate_stretched_io, "simulate stretched io in ml_read_io, ml_write_io");
#endif /* ML_IO_SIMULATE_STRETCHED_ENABLED */

#endif /* ML_IO_TIMEOUTS_ENABLED */

int opensource_kernel = 1;
SYSCTL_INT(_kern, OID_AUTO, opensource_kernel, CTLFLAG_KERN | CTLFLAG_RD | CTLFLAG_LOCKED,
    &opensource_kernel, 0, "Opensource Kernel");

// no pointer authentication on riscv
static int
machdep_ptrauth_enabled SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	const int ret = 0;

	return SYSCTL_OUT(req, &ret, sizeof(ret));
}

SYSCTL_PROC(_machdep, OID_AUTO, ptrauth_enabled,
    CTLTYPE_INT | CTLFLAG_KERN | CTLFLAG_RD,
    0, 0,
    machdep_ptrauth_enabled, "I", "");

static const char _ctrr_type[] = "none";

SYSCTL_STRING(_machdep, OID_AUTO, ctrr_type,
    CTLFLAG_KERN | CTLFLAG_RD | CTLFLAG_LOCKED,
    __DECONST(char *, _ctrr_type), 0,
    "CTRR type supported by hardware/kernel");

#if CONFIG_TELEMETRY && (DEBUG || DEVELOPMENT)
extern unsigned long trap_telemetry_reported_events;
SYSCTL_ULONG(_debug, OID_AUTO, trap_telemetry_reported_events,
    CTLFLAG_RD | CTLFLAG_LOCKED, &trap_telemetry_reported_events,
    "Number of trap telemetry events successfully reported");

extern unsigned long trap_telemetry_capacity_dropped_events;
SYSCTL_ULONG(_debug, OID_AUTO, trap_telemetry_capacity_dropped_events,
    CTLFLAG_RD | CTLFLAG_LOCKED, &trap_telemetry_capacity_dropped_events,
    "Number of trap telemetry events which were dropped due to a full RSB");
#endif /* CONFIG_TELEMETRY && (DEBUG || DEVELOPMENT) */
