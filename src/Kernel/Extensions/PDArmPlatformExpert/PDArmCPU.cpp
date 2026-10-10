#include "PDArmCPU.h"
#include "PDArmGIC.h"
#include "PDSun50iUSB.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <arm/machine_routines.h>
#include <arm/cpu_topology.h>
#include <pexpert/pexpert.h>

#define PD_LOG(...) do { if (ml_get_interrupts_enabled()) { IOLog(__VA_ARGS__); } } while (0)

// PSCI 0.2+: CPU_ON takes the target MPIDR, a physical entry point and a context id.
#define PSCI_FN_VERSION		0x84000000u
#define PSCI_FN64_CPU_ON	0xc4000003u
#define PSCI_FN_SYSTEM_OFF	0x84000008u
#define PSCI_FN_SYSTEM_RESET	0x84000009u

static bool pd_psci_use_smc;
static bool pd_psci_probed;
static bool pd_psci_ok;

static IOCPUInterruptController *gPDCpuIC;
static unsigned int gPDCpuCount = 1;

void
PDArmCPU::setCPUCount(unsigned int count)
{
	gPDCpuCount = (count != 0) ? count : 1;
}

static uint64_t gPDCpuIntEnabled;

static void
pd_enable_cpu_interrupt(IOCPUInterruptController *ic, PDArmCPU *cpu, unsigned int number)
{
    uint64_t bit = 1ULL << (number & 63);

    if (gPDCpuIntEnabled & bit) {
        return;
    }
    gPDCpuIntEnabled |= bit;
    ic->enableCPUInterrupt(cpu);
}

static IOCPUInterruptController *
pd_cpu_ic(IOService *owner)
{
	PDArmCPUInterruptController *ic;

	if (gPDCpuIC != NULL) {
		return gPDCpuIC;
	}
	ic = new PDArmCPUInterruptController;
	if (ic == NULL) {
		return NULL;
	}
	if (ic->initCPUInterruptController((int)gPDCpuCount, (int)gPDCpuCount) != kIOReturnSuccess) {
		return NULL;
	}
	ic->attach(owner);
	ic->registerCPUInterruptController();
	gPDCpuIC = ic;
	PD_LOG("PD-CPU: interrupt controller for %u cpu(s)\n", gPDCpuCount);
	return gPDCpuIC;
}

extern "C" void flush_dcache64(addr64_t addr, unsigned count, int phys);

// secondary entry, copied to a page cleaned to PoC and run with the mmu off
// tf-a wakes the core at el2 on boards with u-boot at el2, so drop to el1 the way the loader does
extern "C" const uint8_t pd_smp_tramp_start[], pd_smp_tramp_lits[], pd_smp_tramp_end[];
__asm__(
	"	.section __TEXT,__text,regular,pure_instructions\n"
	"	.p2align 3\n"
	"	.globl _pd_smp_tramp_start\n"
	"_pd_smp_tramp_start:\n"
	"	msr	DAIFSet, #0xf\n"
	"	msr	OSLAR_EL1, xzr\n"
	"	mrs	x0, CurrentEL\n"
	"	cmp	x0, #(2 << 2)\n"
	"	b.ne	1f\n"
	"	mov	x0, #(1 << 31)\n"
	"	msr	hcr_el2, x0\n"
	"	msr	cntvoff_el2, xzr\n"
	"	mrs	x0, cnthctl_el2\n"
	"	orr	x0, x0, #3\n"
	"	msr	cnthctl_el2, x0\n"
	"	mov	x0, #0x33ff\n"
	"	msr	cptr_el2, x0\n"
	"	msr	hstr_el2, xzr\n"
	"	msr	sctlr_el1, xzr\n"
	"	mov	x0, #0x3c5\n"
	"	msr	spsr_el2, x0\n"
	"	adr	x0, 1f\n"
	"	msr	elr_el2, x0\n"
	"	isb\n"
	"	eret\n"
	"1:\n"
	"	ldr	x20, Lpd_tramp_bootargs\n"
	"	ldr	x21, Lpd_tramp_cpudata\n"
	"	ldr	x16, Lpd_tramp_entry\n"
	"	ldr	x17, Lpd_tramp_marker\n"
	"	mov	w18, #0xd1\n"
	"	str	w18, [x17]\n"
	"	br	x16\n"
	"	.p2align 3\n"
	"	.globl _pd_smp_tramp_lits\n"
	"_pd_smp_tramp_lits:\n"
	"Lpd_tramp_bootargs:	.quad 0\n"
	"Lpd_tramp_cpudata:	.quad 0\n"
	"Lpd_tramp_entry:	.quad 0\n"
	"Lpd_tramp_marker:	.quad 0\n"
	"	.globl _pd_smp_tramp_end\n"
	"_pd_smp_tramp_end:\n"
	"	.text\n");

// the secondary writes 0xd1 here once it runs the trampoline
#define PD_TRAMP_MARKER_OFF	0x100

extern "C" volatile uint32_t pd_smp_marker;
extern "C" volatile uint64_t pd_smp_diag[8];

// SMCC: x0-x3 carry the arguments and come back as results. x4-x17 are not preserved
#define PD_SMCC_CLOBBERS \
        "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", \
        "x12", "x13", "x14", "x15", "x16", "x17", "memory"

static int64_t
pd_psci_call(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3, bool smc)
{
	register uint64_t x0 __asm__ ("x0") = fn;
	register uint64_t x1 __asm__ ("x1") = a1;
	register uint64_t x2 __asm__ ("x2") = a2;
	register uint64_t x3 __asm__ ("x3") = a3;

	if (smc) {
		__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : PD_SMCC_CLOBBERS);
	} else {
		__asm__ volatile ("hvc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : PD_SMCC_CLOBBERS);
	}
	return (int64_t)x0;
}

// the conduit the board's device tree names: an hvc with no el2 underneath is an undefined instruction
static int
pd_psci_dt_method(void)
{
	IORegistryEntry *psci = IORegistryEntry::fromPath("/psci", gIODTPlane);
	OSData *method = psci != NULL ? OSDynamicCast(OSData, psci->getProperty("method")) : NULL;
	int smc = -1;

	if (method != NULL && method->getLength() >= 3) {
		if (strncmp((const char *)method->getBytesNoCopy(), "smc", 3) == 0) {
			smc = 1;
		} else if (strncmp((const char *)method->getBytesNoCopy(), "hvc", 3) == 0) {
			smc = 0;
		}
	}
	OSSafeReleaseNULL(psci);
	return smc;
}

static bool
pd_psci_available(void)
{
	if (!pd_psci_probed) {
		int64_t v;
		int dt = pd_psci_dt_method();
		const char *from;

		if (dt >= 0) {
			pd_psci_use_smc = dt == 1;
			v = pd_psci_call(PSCI_FN_VERSION, 0, 0, 0, pd_psci_use_smc);
			from = "/psci";
		} else if (PDSun50i_isPlatform()) {
			// tf-a owns psci on sun50i, an hvc would land in u-boot's stale el2 vectors
			pd_psci_use_smc = true;
			v = pd_psci_call(PSCI_FN_VERSION, 0, 0, 0, true);
			from = "sun50i";
		} else {
			v = pd_psci_call(PSCI_FN_VERSION, 0, 0, 0, false);
			if (v < 0) {
			    v = pd_psci_call(PSCI_FN_VERSION, 0, 0, 0, true);
			    pd_psci_use_smc = (v >= 0);
			}
			from = "probe";
		}

		pd_psci_ok = (v >= 0);
		pd_psci_probed = true;
		PD_LOG("PD-CPU: PSCI version 0x%llx (%u.%u) via %s, from %s\n", (unsigned long long)v,
		    (unsigned)(((uint64_t)v >> 16) & 0x7fff), (unsigned)((uint64_t)v & 0xffff),
		    pd_psci_use_smc ? "smc" : "hvc", from);
	}
	return pd_psci_ok;
}

// reboot(2) and shutdown end here, psci only returns when the firmware lacks the call
int
PDArmCPU::systemReset(bool off)
{
	pd_psci_available();
	pd_psci_call(off ? PSCI_FN_SYSTEM_OFF : PSCI_FN_SYSTEM_RESET, 0, 0, 0, pd_psci_use_smc);
	return -1;
}

#undef super
#define super IOCPU

OSDefineMetaClassAndStructors(PDArmCPU, IOCPU);

IOService *
PDArmCPU::probe(IOService *provider, SInt32 *score)
{
	return this;
}

bool
PDArmCPU::startCommon(void)
{
	if (startCommonCompleted) {
		return true;
	}

	PD_LOG("PD-CPU: startCommon begin\n");
	cpuIC = pd_cpu_ic(this);
	if (cpuIC == NULL) {
		return false;
	}

	setCPUState(kIOCPUStateUninitalized);
	PD_LOG("PD-CPU: initCPU\n");
	initCPU(true);
	PD_LOG("PD-CPU: registerService\n");
	registerService();
	PD_LOG("PD-CPU: startCommon done\n");

	startCommonCompleted = true;
	return true;
}

bool
PDArmCPU::start(IOService *provider)
{
	if (!super::start(provider)) {
		return false;
	}
	return startCommon();
}

void
PDArmCPU::initCPU(bool boot)
{
	if (cpuIC == NULL) {
		cpuIC = pd_cpu_ic(this);
	}
	if (cpuIC == NULL) {
		PD_LOG("PD-CPU: initCPU cpu %u without a controller\n", pdCpuNumber);
		return;
	}

	if (pdCpuNumber != 0) {
		PDArmGIC_init_cpu(pdCpuNumber);
		pd_enable_cpu_interrupt(cpuIC, this, pdCpuNumber);
		PDArmGIC_enable_cpu(pdCpuNumber);
		setCPUState(kIOCPUStateRunning);
		// xnu clears SIGPdisabled only when a CPU first takes an IPI,
		// and cpu_signal() fails until it does. This one waits until we unmask
		PDArmGIC_send_ipi(pdPhysId);
		return;
	}

	pd_enable_cpu_interrupt(cpuIC, this, pdCpuNumber);
	PDArmGIC_enable();
	setCPUState(kIOCPUStateRunning);
	PDArmGIC_send_ipi(pdPhysId);
}

void
PDArmCPU::quiesceCPU(void)
{
	/* Single boot CPU, never quiesced on virt. */
}

kern_return_t
PDArmCPU::startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr)
{
	// the marker store proves the cpu reached the trampoline, a fault there runs mmu-off and prints nothing
	const size_t trampLen = (size_t)(pd_smp_tramp_end - pd_smp_tramp_start);
	const size_t litOff = (size_t)(pd_smp_tramp_lits - pd_smp_tramp_start);
	IOBufferMemoryDescriptor *buf;
	uint64_t *lit;
	uint8_t *page;
	int64_t ret;

	if (start_paddr == 0 || arg_paddr == 0) {
		PD_LOG("PD-CPU: cpu %u start without entry/args\n", pdCpuNumber);
		return KERN_FAILURE;
	}
	if (!pd_psci_available()) {
		PD_LOG("PD-CPU: cpu %u not started, PSCI_VERSION failed\n", pdCpuNumber);
		return KERN_FAILURE;
	}

	buf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIOMemoryPhysicallyContiguous | kIODirectionInOut,
	    0x1000, 0xfffffffffffff000ULL);
	if (buf == NULL) {
		return KERN_RESOURCE_SHORTAGE;
	}

	uint64_t tramp_pa = (uint64_t)buf->getPhysicalAddress();

	page = (uint8_t *)buf->getBytesNoCopy();
	if (trampLen > PD_TRAMP_MARKER_OFF) {
		buf->release();
		return KERN_FAILURE;
	}
	memcpy(page, pd_smp_tramp_start, trampLen);
	lit = (uint64_t *)(page + litOff);
	lit[0] = (uint64_t)ml_static_vtop((vm_offset_t)PE_state.bootArgs);
	lit[1] = (uint64_t)arg_paddr;
	lit[2] = (uint64_t)start_paddr;
	lit[3] = tramp_pa + PD_TRAMP_MARKER_OFF;
	*(volatile uint32_t *)(page + PD_TRAMP_MARKER_OFF) = 0;

	// by VA: a phys-mode flush panics for any PA outside the kernel's single physical window
	flush_dcache64((addr64_t)(uintptr_t)page, PD_TRAMP_MARKER_OFF + 0x40, 0);

	pd_smp_marker = 0;
	flush_dcache64((addr64_t)ml_static_vtop((vm_offset_t)&pd_smp_marker),
	    sizeof(pd_smp_marker), 1);
	pd_smp_diag[0] = pd_smp_diag[1] = pd_smp_diag[2] = pd_smp_diag[3] = 0;
	flush_dcache64((addr64_t)ml_static_vtop((vm_offset_t)&pd_smp_diag[0]),
	    sizeof(pd_smp_diag), 1);

	ret = pd_psci_call(PSCI_FN64_CPU_ON, pdPhysId, tramp_pa, pdCpuNumber,
	    pd_psci_use_smc);
	PD_LOG("PD-CPU: PSCI CPU_ON cpu %u mpidr 0x%x tramp 0x%llx entry 0x%llx args 0x%llx data 0x%llx -> %lld\n",
	    pdCpuNumber, pdPhysId, (unsigned long long)tramp_pa,
	    (unsigned long long)start_paddr, (unsigned long long)lit[0],
	    (unsigned long long)arg_paddr, (long long)ret);

	if (ret == 0) {
		unsigned int i;

		// the page is cacheable now and the secondary stores with the mmu off, drop our line before each read
		for (i = 0; i < 100; i++) {
			flush_dcache64((addr64_t)(uintptr_t)(page + PD_TRAMP_MARKER_OFF), sizeof(uint32_t), 0);
			if (*(volatile uint32_t *)(page + PD_TRAMP_MARKER_OFF) == 0xd1) {
				break;
			}
			IOSleep(10);
		}
		PD_LOG("PD-CPU: cpu %u trampoline marker %s after %u ms\n", pdCpuNumber,
		    (*(volatile uint32_t *)(page + PD_TRAMP_MARKER_OFF) == 0xd1) ? "reached" : "NOT reached",
		    i * 10);

		for (i = 0; i < 100; i++) {
			flush_dcache64((addr64_t)ml_static_vtop((vm_offset_t)&pd_smp_marker),
			    sizeof(pd_smp_marker), 1);
			if (pd_smp_marker == 3) {
				break;
			}
			IOSleep(10);
		}
		PD_LOG("PD-CPU: cpu %u xnu progress marker %u after %u ms\n", pdCpuNumber,
		    (unsigned)pd_smp_marker, i * 10);

		// The secondary computes its virtual base from the boot args.
		// Compare that against the bias this CPU is actually running with
		flush_dcache64((addr64_t)ml_static_vtop((vm_offset_t)&pd_smp_diag[0]),
		    sizeof(pd_smp_diag), 1);
		PD_LOG("PD-CPU: cpu %u tramp target 0x%llx lr 0x%llx virtbase 0x%llx "
		    "physbase 0x%llx; running bias 0x%llx\n", pdCpuNumber,
		    (unsigned long long)pd_smp_diag[0], (unsigned long long)pd_smp_diag[1],
		    (unsigned long long)pd_smp_diag[2], (unsigned long long)pd_smp_diag[3],
		    (unsigned long long)((uintptr_t)&pd_smp_marker
		    - (uintptr_t)ml_static_vtop((vm_offset_t)&pd_smp_marker)));
		PD_LOG("PD-CPU: cpu %u secondary sp 0x%llx cpudata 0x%llx (handed 0x%llx)\n",
		    pdCpuNumber, (unsigned long long)pd_smp_diag[4],
		    (unsigned long long)pd_smp_diag[5], (unsigned long long)arg_paddr);
	}

	return ret == 0 ? KERN_SUCCESS : KERN_FAILURE;
}

void
PDArmCPU::signalCPU(IOCPU *target)
{
	PDArmCPU *t = OSDynamicCast(PDArmCPU, target);

	if (t != NULL) {
		PDArmGIC_send_ipi(t->pdPhysId);
	}
}

// Bring up one processor object: register it with xnu so PE_cpu_start() can find it,
// then either finish boot-CPU setup or wait to be started
bool
PDArmCPU::startForCPU(unsigned int cpu, uint32_t phys_id, bool boot)
{
	ml_processor_info_t info;
	processor_t proc = NULL;
	ipi_handler_t ipi = NULL;
	perfmon_interrupt_handler_func pmi = NULL;

	pdCpuNumber = cpu;
	pdPhysId = phys_id;
	pdIsBoot = boot;
	setCPUNumber(cpu);

	// Map this CPU's redistributor frame here, on the boot CPU:
	// the secondary reaches initCPU() with interrupts masked, where mapping is not safe
	if (!PDArmGIC_map_cpu(cpu, phys_id)) {
		PD_LOG("PD-CPU: no redistributor frame for cpu %u\n", cpu);
		return false;
	}

	// Must exist before registering:
	// xnu can call back into initCPU() as soon as this processor is known to it
	cpuIC = pd_cpu_ic(this);
	if (cpuIC == NULL) {
		PD_LOG("PD-CPU: no interrupt controller for cpu %u\n", cpu);
		return false;
	}

	memset(&info, 0, sizeof(info));
	info.cpu_id = (cpu_id_t)this;
	info.phys_id = phys_id;
	info.log_id = cpu;
	info.cluster_id = 0;
	info.cluster_type = CLUSTER_TYPE_SMP;
	info.start_paddr = 0;

	if (ml_processor_register(&info, &proc, &ipi, &pmi) != KERN_SUCCESS) {
		PD_LOG("PD-CPU: ml_processor_register failed for cpu %u\n", cpu);
		return false;
	}
	if (boot) {
		return startCommon();
	}

	setCPUState(kIOCPUStateUninitalized);
	registerService();
	return true;
}

void
PDArmCPU::haltCPU(void)
{
	/* Not required. */
}

const OSSymbol *
PDArmCPU::getCPUName(void)
{
	return OSSymbol::withCStringNoCopy("Primary0");
}

#undef super
#define super IOCPUInterruptController

OSDefineMetaClassAndStructors(PDArmCPUInterruptController, IOCPUInterruptController);

IOReturn
PDArmCPUInterruptController::handleInterrupt(void *refCon, IOService *nub, int source)
{
	IOInterruptVector *vector = &vectors[0];
	if (!vector->interruptRegistered) {
		return kIOReturnInvalid;
	}

	vector->handler(vector->target, refCon, vector->nub, source);
	return kIOReturnSuccess;
}
