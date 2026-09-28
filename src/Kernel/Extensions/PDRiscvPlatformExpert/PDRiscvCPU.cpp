#include "PDRiscvCPU.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <riscv/machine_routines.h>
#include <pexpert/pexpert.h>

#define PD_LOG(...) do { if (ml_get_interrupts_enabled()) { IOLog(__VA_ARGS__); } } while (0)

// the kernel owns what arm routes through this kext, cpu_start brings up secondaries with sbi hsm
// ipis travel as sbi send_ipi to ssip, the timer is sbi set_timer or sstc under pexpert's empty tbd ops

static IOCPUInterruptController *gPDCpuIC;
static unsigned int gPDCpuCount = 1;

void
PDRiscvCPU::setCPUCount(unsigned int count)
{
	gPDCpuCount = (count != 0) ? count : 1;
}

static IOCPUInterruptController *
pd_cpu_ic(IOService *owner)
{
	PDRiscvCPUInterruptController *ic;

	if (gPDCpuIC != NULL) {
		return gPDCpuIC;
	}
	ic = new PDRiscvCPUInterruptController;
	if (ic == NULL) {
		return NULL;
	}
	// one source per cpu, each hart's sei comes in as its own cpu number
	if (ic->initCPUInterruptController((int)gPDCpuCount, (int)gPDCpuCount) != kIOReturnSuccess) {
		ic->release();
		return NULL;
	}
	ic->attach(owner);
	ic->registerCPUInterruptController();
	gPDCpuIC = ic;
	return gPDCpuIC;
}

#undef super
#define super IOCPU

OSDefineMetaClassAndStructors(PDRiscvCPU, IOCPU);

IOService *
PDRiscvCPU::probe(IOService *provider, SInt32 *score)
{
	return this;
}

void
PDRiscvCPU::free(void)
{
	OSSafeReleaseNULL(pdName);
	super::free();
}

bool
PDRiscvCPU::startCommon(void)
{
	if (startCommonCompleted) {
		return true;
	}

	cpuIC = pd_cpu_ic(this);
	if (cpuIC == NULL) {
		return false;
	}

	// registering the boot cpu already ran initCPU through cpu_machine_init
	if (!pdInterruptInstalled) {
		initCPU(true);
	}
	registerService();

	startCommonCompleted = true;
	return true;
}

// runs on the cpu itself from cpu_machine_init, with interrupts masked
void
PDRiscvCPU::initCPU(bool boot)
{
	if (cpuIC == NULL) {
		cpuIC = pd_cpu_ic(this);
	}
	if (cpuIC == NULL) {
		PD_LOG("PDRiscvCPU: initCPU cpu %u without a controller\n", pdCpuNumber);
		return;
	}

	// ml_install_interrupt_handler goes into this hart's cpu data, which survives sleep
	// enabling twice would count this cpu twice toward the controller's cpusRunning
	if (!pdInterruptInstalled) {
		pdInterruptInstalled = true;
		cpuIC->enableCPUInterrupt(this);
	}
	setCPUState(kIOCPUStateRunning);
}

void
PDRiscvCPU::quiesceCPU(void)
{
	// no sleep support yet, nothing to save
}

kern_return_t
PDRiscvCPU::startCPU(vm_offset_t, vm_offset_t)
{
	// cpu_start issues sbi hart_start itself and never calls down here
	return KERN_SUCCESS;
}

// register one processor so the kernel can find it by cpu number and hart id
// the kernel boots it from inside ml_processor_register, the boot cpu synchronously
bool
PDRiscvCPU::startForCPU(unsigned int cpu, uint32_t hart, bool boot)
{
	ml_processor_info_t info;
	processor_t proc = NULL;
	ipi_handler_t ipi = NULL;
	perfmon_interrupt_handler_func pmi = NULL;
	char name[16];

	pdCpuNumber = cpu;
	pdHartId = hart;
	pdIsBoot = boot;
	setCPUNumber(cpu);

	snprintf(name, sizeof(name), boot ? "Primary%u" : "Secondary%u", cpu);
	pdName = OSSymbol::withCString(name);

	// must exist before registering, the hart runs initCPU as soon as xnu knows it
	cpuIC = pd_cpu_ic(this);
	if (cpuIC == NULL) {
		PD_LOG("PDRiscvCPU: no interrupt controller for cpu %u\n", cpu);
		return false;
	}

	// a secondary may reach initCPU and mark itself running before registration returns
	setCPUState(kIOCPUStateUninitalized);

	memset(&info, 0, sizeof(info));
	info.cpu_id = (cpu_id_t)this;
	info.phys_id = hart;
	info.log_id = cpu;
	info.cluster_id = 0;
	info.cluster_type = CLUSTER_TYPE_SMP;
	info.start_paddr = 0;

	if (ml_processor_register(&info, &proc, &ipi, &pmi) != KERN_SUCCESS) {
		PD_LOG("PDRiscvCPU: ml_processor_register failed for cpu %u hart %u\n", cpu, hart);
		return false;
	}
	machProcessor = proc;
	ipi_handler = ipi;

	if (boot) {
		return startCommon();
	}
	registerService();
	return true;
}

void
PDRiscvCPU::haltCPU(void)
{
	// not required
}

const OSSymbol *
PDRiscvCPU::getCPUName(void)
{
	// the symbol stays owned by this cpu, callers do not release it
	return pdName;
}

#undef super
#define super IOCPUInterruptController

OSDefineMetaClassAndStructors(PDRiscvCPUInterruptController, IOCPUInterruptController);

// every cpu's sei lands here, vector 0 is the plic and it wants to know which hart took it
IOReturn
PDRiscvCPUInterruptController::handleInterrupt(void *refCon, IOService *nub, int source)
{
	IOInterruptVector *vector = &vectors[0];

	if (!vector->interruptRegistered) {
		return kIOReturnInvalid;
	}

	vector->handler(vector->target, vector->refCon, vector->nub, source);
	return kIOReturnSuccess;
}
