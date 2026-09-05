#include "PDArmCPU.h"
#include "PDArmGIC.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>

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

	IOLog("PD-CPU: startCommon begin\n");
	cpuIC = new PDArmCPUInterruptController;
	if (cpuIC == NULL) {
		return false;
	}

	IOLog("PD-CPU: initCPUInterruptController\n");
	if (cpuIC->initCPUInterruptController(1) != kIOReturnSuccess) {
		return false;
	}

	IOLog("PD-CPU: attach\n");
	cpuIC->attach(this);
	IOLog("PD-CPU: registerCPUInterruptController\n");
	cpuIC->registerCPUInterruptController();
	IOLog("PD-CPU: registered\n");

	setCPUState(kIOCPUStateUninitalized);
	IOLog("PD-CPU: initCPU\n");
	initCPU(true);
	IOLog("PD-CPU: registerService\n");
	registerService();
	IOLog("PD-CPU: startCommon done\n");

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
	/* cpu_data->interrupt_handler is live as of the call above, so GIC delivery
	 * can start now. It must not start earlier - sleh_irq branches through that
	 * pointer - and it must not be deferred to initPlatformInterruptsLate(),
	 * because the timer PPI is what wakes anything that sleeps between here and
	 * there. Returns false on non-GIC platforms, which is fine. */
	PDArmGIC_enable();
	setCPUState(kIOCPUStateRunning);
}

void
PDArmCPU::quiesceCPU(void)
{
	/* Single boot CPU, never quiesced on virt. */
}

kern_return_t
PDArmCPU::startCPU(vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/)
{
	/* No secondary CPUs are started on the virt target. */
	return KERN_FAILURE;
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
