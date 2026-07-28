#include "PDArmCPU.h"
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

	cpuIC = new PDArmCPUInterruptController;
	if (cpuIC == NULL) {
		return false;
	}

	if (cpuIC->initCPUInterruptController(1) != kIOReturnSuccess) {
		return false;
	}

	cpuIC->attach(this);
	cpuIC->registerCPUInterruptController();

	setCPUState(kIOCPUStateUninitalized);
	initCPU(true);
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
