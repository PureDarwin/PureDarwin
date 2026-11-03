#include "PDArmCPU.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#if defined(__arm__) && !defined(__arm64__)
#include <IOKit/IODeviceTreeSupport.h>
#include <machine/machine_routines.h>

extern "C" {
// the gic pexpert maps on the boot core, and the kernel's low reset vector page
extern vm_offset_t pe_gic_gicd, pe_gic_gicc;
extern uint32_t pe_gic_timer_irq;
extern unsigned long gPhysBase;
kern_return_t processor_start(processor_t processor);
vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
}

// every core shares the boot core's interrupt controller, sized for all of them
static IOCPUInterruptController *sSharedIC;
static unsigned sNumCPUs = 1;
static PDArmCPU::StartMethod sStartMethod;
// the kernel's cpu limit for the generic a7 class
#define kMaxCPUs 4
#endif

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

#if defined(__arm__) && !defined(__arm64__)
	if (sSharedIC != NULL) {
		cpuIC = sSharedIC;
	} else
#endif
	{
		cpuIC = new PDArmCPUInterruptController;
		if (cpuIC == NULL) {
			return false;
		}

#if defined(__arm__) && !defined(__arm64__)
		if (cpuIC->initCPUInterruptController(sNumCPUs) != kIOReturnSuccess) {
#else
		if (cpuIC->initCPUInterruptController(1) != kIOReturnSuccess) {
#endif
			return false;
		}

		cpuIC->attach(this);
		cpuIC->registerCPUInterruptController();
#if defined(__arm__) && !defined(__arm64__)
		sSharedIC = cpuIC;
#endif
	}

	setCPUState(kIOCPUStateUninitalized);
	// a secondary installs its interrupt handler itself, once it runs
	if (getCPUNumber() == 0) {
		initCPU(true);
	}
	registerService();

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
PDArmCPU::initCPU(bool /*boot*/)
{
	cpuIC->enableCPUInterrupt(this);
	setCPUState(kIOCPUStateRunning);
}

void
PDArmCPU::quiesceCPU(void)
{
	/* Single boot CPU, never quiesced on virt. */
}

#if defined(__arm__) && !defined(__arm64__)
// the kexts build for armv6, so armv7 instructions this a7-only path needs are emitted raw
static inline void
pdDSB(void)
{
	asm volatile (
#if defined(__thumb__)
		".short 0xf3bf\n\t.short 0x8f4f"        // dsb sy
#else
		".long 0xf57ff04f"                       // dsb sy
#endif
		::: "memory");
}

// psci cpu_on through qemu's hvc conduit
static int32_t
pdPsciCPUOn(uint32_t mpidr, uint32_t entry)
{
	register uint32_t r0 asm("r0") = 0x84000003;
	register uint32_t r1 asm("r1") = mpidr;
	register uint32_t r2 asm("r2") = entry;
	register uint32_t r3 asm("r3") = 0;

	asm volatile (
#if defined(__thumb__)
		".short 0xf7e0\n\t.short 0x8000"        // hvc #0
#else
		".long 0xe1400070"                       // hvc #0
#endif
		: "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory");
	return (int32_t)r0;
}

// rk3506 boot rom: the other cores wfe until sram+4 holds 0xdeadbeaf, then jump to sram+8
#define kRockchipSRAM        0xfff80000u
#define kRockchipPenOffset   0x800

// the pen they jump to, from the kernel's reset vector: cntfrq, then cntvoff zeroed from
// monitor mode with scr.ns set, back to svc and on to the entry in its last word
static const uint32_t kRockchipPen[] = {
	0xe59f0038,     // ldr r0, cntfrq
	0xee0e0f10,     // mcr p15, 0, r0, c14, c0, 0
	0xf1020016,     // cps #0x16
	0xee111f11,     // mrc p15, 0, r1, c1, c1, 0
	0xe3811001,     // orr r1, r1, #1
	0xee011f11,     // mcr p15, 0, r1, c1, c1, 0
	0xf57ff06f,     // isb
	0xe3a00000,     // mov r0, #0
	0xe3a01000,     // mov r1, #0
	0xec410f4e,     // mcrr p15, 4, r0, r1, c14
	0xee111f11,     // mrc p15, 0, r1, c1, c1, 0
	0xe3c11001,     // bic r1, r1, #1
	0xee011f11,     // mcr p15, 0, r1, c1, c1, 0
	0xf57ff06f,     // isb
	0xf1020013,     // cps #0x13
	0xe59ff000,     // ldr pc, entry
	0x016e3600,     // cntfrq: 24 MHz
	0,              // entry
};

// pmu: per core auto power-down and soft power switch (hiword masked), and the cluster's power
// state; grf_core: per core standbywfe in bits 2:0 and standbywfi in bits 6:4
#define kRockchipPMU             0xff900000u
#define kPMUClusterPwrSt         0x101c
#define kPMUCPUAutoPwrCon(n)     (0x1040 + 4 * (n))
#define kPMUCPUPwrSftCon(n)      (0x1050 + 4 * (n))
#define kRockchipGRFCoreStatus   0xff840060u

// a core the firmware powered off stays off across a global reset: power it back on so it
// reruns the rom and parks in the mailbox loop
static void
pdRockchipPowerOn(unsigned ncpus)
{
	vm_offset_t pmu = ml_io_map(kRockchipPMU, 0x2000);
	vm_offset_t grf = ml_io_map(kRockchipGRFCoreStatus & ~0xfffu, PAGE_SIZE);
	volatile uint32_t *status;

	if (pmu == 0 || grf == 0) {
		return;
	}
	status = (volatile uint32_t *)(grf + (kRockchipGRFCoreStatus & 0xfff));
	for (unsigned n = 1; n < ncpus; n++) {
		if ((*(volatile uint32_t *)(pmu + kPMUClusterPwrSt) & (1u << (9 + n))) == 0) {
			continue;
		}
		IOLog("PDArmCPU: cpu %u was powered off, powering it on\n", n);
		*(volatile uint32_t *)(pmu + kPMUCPUAutoPwrCon(n)) = 0xffff0000;
		*(volatile uint32_t *)(pmu + kPMUCPUPwrSftCon(n)) = 0x00010000;
		for (unsigned t = 0; t < 1000 && (*(volatile uint32_t *)(pmu + kPMUClusterPwrSt) & (1u << (9 + n))); t++) {
			IODelay(10);
		}
	}
	// the rom needs a moment to park
	for (unsigned t = 0; t < 1000; t++) {
		uint32_t want = ((1u << ncpus) - 1) & ~1u;

		if ((*status & want) == want) {
			break;
		}
		IODelay(10);
	}
}

static kern_return_t
pdRockchipRelease(uint32_t entry)
{
	vm_offset_t sram = ml_io_map(kRockchipSRAM, PAGE_SIZE);
	volatile uint32_t *pen;
	const unsigned n = sizeof(kRockchipPen) / sizeof(kRockchipPen[0]);

	if (sram == 0) {
		return KERN_FAILURE;
	}
	pen = (volatile uint32_t *)(sram + kRockchipPenOffset);
	for (unsigned i = 0; i < n - 1; i++) {
		pen[i] = kRockchipPen[i];
	}
	pen[n - 1] = entry;
	*(volatile uint32_t *)(sram + 8) = kRockchipSRAM + kRockchipPenOffset;
	pdDSB();
	*(volatile uint32_t *)(sram + 4) = 0xdeadbeaf;
	pdDSB();
	asm volatile (
#if defined(__thumb__)
		".short 0xbf40"                          // sev
#else
		".long 0xe320f004"                       // sev
#endif
		::: "memory");
	return KERN_SUCCESS;
}

unsigned
PDArmCPU::startSMP(StartMethod method)
{
	const ml_topology_info_t *topo = ml_get_topology_info();
	PDArmCPU *cpus[kMaxCPUs] = { };
	unsigned n = topo->num_cpus;

	// ipis need the gic's group 1, which a boot under op-tee does not give us
	if (method == kStartNone || pe_gic_timer_irq != 0 || pe_gic_gicd == 0 || n > kMaxCPUs) {
		n = 1;
	}
	sNumCPUs = n;
	sStartMethod = method;

	for (unsigned i = 0; i < n; i++) {
		char path[16];
		OSData *reg;
		IORegistryEntry *node;

		cpus[i] = new PDArmCPU;
		if (cpus[i] == NULL || !cpus[i]->init()) {
			return 0;
		}
		cpus[i]->setCPUNumber(i);
		snprintf(path, sizeof(path), "/cpus/cpu%u", i);
		node = IORegistryEntry::fromPath(path, gIODTPlane);
		reg = node != NULL ? OSDynamicCast(OSData, node->getProperty("reg")) : NULL;
		cpus[i]->fMPIDR = (reg != NULL && reg->getLength() >= 4) ?
		    *(const uint32_t *)reg->getBytesNoCopy() : topo->cpus[i].phys_id;
		OSSafeReleaseNULL(node);
		cpus[i]->attach(0);
		if (!cpus[i]->startCommon()) {
			return 0;
		}
	}

	// mach needs every core registered before any of them starts
	for (unsigned i = 0; i < n; i++) {
		ml_processor_info_t info = { };
		processor_t proc = NULL;
		ipi_handler_t ipi = NULL;
		perfmon_interrupt_handler_func pmi = NULL;

		info.cpu_id = (cpu_id_t)cpus[i];
		info.phys_id = topo->cpus[i].phys_id;
		info.log_id = topo->cpus[i].cpu_id;
		info.cluster_id = 0;
		info.cluster_type = CLUSTER_TYPE_SMP;
		if (ml_processor_register(&info, &proc, &ipi, &pmi) != KERN_SUCCESS) {
			panic("PDArmCPU: could not register cpu %u", i);
		}
		cpus[i]->machProcessor = proc;
		cpus[i]->ipi_handler = ipi;
	}
	for (unsigned i = 1; i < n; i++) {
		kern_return_t kr = processor_start(cpus[i]->machProcessor);

		IOLog("PDArmCPU: cpu %u (mpidr 0x%x) start %d\n", i, cpus[i]->fMPIDR, kr);
	}
	return n;
}

void
PDArmCPU::handleIPI(void)
{
	if (ipi_handler != NULL) {
		ipi_handler();
	}
}
#endif

kern_return_t
PDArmCPU::startCPU(vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/)
{
#if defined(__arm__) && !defined(__arm64__)
	// a core enters the kernel's low reset vector in svc with the mmu off, and finds its cpu
	// data from its mpidr
	if (getCPUNumber() != 0 && sStartMethod == kStartPSCI) {
		int32_t r = pdPsciCPUOn(fMPIDR, (uint32_t)gPhysBase);

		if (r != 0) {
			IOLog("PDArmCPU: psci cpu_on 0x%x returned %d\n", fMPIDR, r);
			return KERN_FAILURE;
		}
		return KERN_SUCCESS;
	}
	// the rom's one flag frees every parked core, so they go together once the last one
	// starts, when mach has set every core's reset handler
	if (getCPUNumber() != 0 && sStartMethod == kStartRockchipMailbox) {
		if (getCPUNumber() != sNumCPUs - 1) {
			return KERN_SUCCESS;
		}
		pdRockchipPowerOn(sNumCPUs);
		return pdRockchipRelease((uint32_t)gPhysBase);
	}
#endif
	// other boards start no secondary cores
	return KERN_FAILURE;
}

void
PDArmCPU::haltCPU(void)
{
	/* Not required. */
}

void
PDArmCPU::signalCPU(IOCPU *target)
{
#if defined(__arm__) && !defined(__arm64__)
	PDArmCPU *t = OSDynamicCast(PDArmCPU, target);

	if (sNumCPUs > 1 && t != NULL) {
		// sgi 0 to the target's cpu interface, nsatt so a secure write reaches a group 1 sgi
		pdDSB();
		*(volatile uint32_t *)(pe_gic_gicd + 0xf00) = (1u << (16 + (t->fMPIDR & 0xff))) | (1u << 15);
		return;
	}
#endif
	super::signalCPU(target);
}

const OSSymbol *
PDArmCPU::getCPUName(void)
{
#if defined(__arm__) && !defined(__arm64__)
	static const char *names[] = { "Primary0", "Secondary1", "Secondary2", "Secondary3" };

	if (getCPUNumber() < sizeof(names) / sizeof(names[0])) {
		return OSSymbol::withCStringNoCopy(names[getCPUNumber()]);
	}
#endif
	return OSSymbol::withCStringNoCopy("Primary0");
}

#undef super
#define super IOCPUInterruptController

OSDefineMetaClassAndStructors(PDArmCPUInterruptController, IOCPUInterruptController);

IOReturn
PDArmCPUInterruptController::handleInterrupt(void *refCon, IOService *nub, int source)
{
#if defined(__arm__) && !defined(__arm64__)
	// the generic a7 gic: acknowledge, and hand sgis to the interrupted core's ipi handler
	if (sNumCPUs > 1) {
		uint32_t iar = *(volatile uint32_t *)(pe_gic_gicc + 0x0c);
		uint32_t id = iar & 0x3ff;
		PDArmCPU *cpu = OSDynamicCast(PDArmCPU, nub);

		if (id >= 1020) {
			return kIOReturnSuccess;
		}
		*(volatile uint32_t *)(pe_gic_gicc + 0x10) = iar;
		if (id < 16) {
			if (cpu != NULL) {
				cpu->handleIPI();
			}
			return kIOReturnSuccess;
		}
	}
#endif
	IOInterruptVector *vector = &vectors[0];
	if (!vector->interruptRegistered) {
		return kIOReturnInvalid;
	}

	vector->handler(vector->target, refCon, vector->nub, source);
	return kIOReturnSuccess;
}
