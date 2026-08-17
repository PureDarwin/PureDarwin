/*
 * PDAppleAIC - the Apple Interrupt Controller, as an IOInterruptController.
 */
#include "PDAppleAIC.h"
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>

/* AIC registers; see pexpert/pexpert/arm/AIC.h for the full map. */
#define kAICInfo                 0x0004
#define kAICInfoNumIRQ(v)        ((v) & 0x3FF)
#define kAICInfoNumCPU(v)        ((((v) >> 16) & 0x1F) + 1)
#define kAICEvent                0x2004
#define kAICEventType(v)         (((v) >> 16) & 0x7)
#define kAICEventTypeSpurious    0
#define kAICEventTypeExtInt      1
#define kAICEventNum(v)          ((v) & 0x3FF)
#define kAICTargetCPU(n)         (0x3000 + (n) * 4)
#define kAICIntMaskSet(n)        (0x4100 + (n) * 4)
#define kAICIntMaskClr(n)        (0x4180 + (n) * 4)

class PDAppleAIC : public IOInterruptController
{
	OSDeclareDefaultStructors(PDAppleAIC)

private:
	IOMemoryMap      *aicMap;
	volatile uint8_t *aic;
	uint32_t          numIRQs;
	uint32_t          numCPUs;

	uint32_t
	read32(uint32_t off) const
	{
		return *(volatile uint32_t *)(aic + off);
	}
	void
	write32(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(aic + off) = val;
	}

public:
	bool     initAIC(void);

	IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
	bool     vectorCanBeShared(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	int      getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void     disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void     enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
};

#define super IOInterruptController
OSDefineMetaClassAndStructors(PDAppleAIC, IOInterruptController);

static PDAppleAIC *gAIC;

/*
 * The AIC's "reg" is an offset into the arm-io range, exactly as pexpert reads
 * it in pe_arm_map_interrupt_controller(). The device tree is walked directly
 * rather than waiting for a nub, because this runs while the platform expert
 * is still starting.
 */
static bool
aic_find_registers(IOPhysicalAddress *outPhys, IOByteCount *outSize)
{
	IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
	if (armio == NULL) {
		IOLog("PDAppleAIC: no /arm-io node in the device tree\n");
		return false;
	}

	OSData *ranges = OSDynamicCast(OSData, armio->getProperty("ranges"));
	if (ranges == NULL || ranges->getLength() < 2 * sizeof(uint64_t)) {
		IOLog("PDAppleAIC: /arm-io has no usable ranges property\n");
		armio->release();
		return false;
	}
	/* child address, parent (physical) address, size */
	uint64_t socPhys = ((const uint64_t *)ranges->getBytesNoCopy())[1];

	IORegistryEntry *aicNode = NULL;
	OSIterator *children = armio->getChildIterator(gIODTPlane);
	if (children != NULL) {
		while (OSObject *next = children->getNextObject()) {
			IORegistryEntry *child = OSDynamicCast(IORegistryEntry, next);
			if (child == NULL) {
				continue;
			}
			OSData *kind = OSDynamicCast(OSData, child->getProperty("interrupt-controller"));
			if (kind != NULL &&
			    strncmp((const char *)kind->getBytesNoCopy(), "master", kind->getLength()) == 0) {
				aicNode = child;
				break;
			}
		}
		children->release();
	}
	armio->release();

	if (aicNode == NULL) {
		IOLog("PDAppleAIC: no interrupt-controller/master node under /arm-io\n");
		return false;
	}

	OSData *reg = OSDynamicCast(OSData, aicNode->getProperty("reg"));
	if (reg == NULL || reg->getLength() < 2 * sizeof(uint64_t)) {
		IOLog("PDAppleAIC: interrupt controller has no usable reg property\n");
		return false;
	}
	const uint64_t *r = (const uint64_t *)reg->getBytesNoCopy();

	*outPhys = (IOPhysicalAddress)(socPhys + r[0]);
	*outSize = (IOByteCount)r[1];
	return true;
}

bool
PDAppleAIC::initAIC(void)
{
	IOPhysicalAddress phys = 0;
	IOByteCount       size = 0;

	if (!aic_find_registers(&phys, &size)) {
		return false;
	}

	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		phys, size, kIODirectionOutIn);
	if (desc == NULL) {
		return false;
	}
	aicMap = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (aicMap == NULL) {
		IOLog("PDAppleAIC: failed to map registers at 0x%llx\n", (uint64_t)phys);
		return false;
	}
	aic = (volatile uint8_t *)aicMap->getVirtualAddress();

	uint32_t info = read32(kAICInfo);
	numIRQs = kAICInfoNumIRQ(info);
	numCPUs = kAICInfoNumCPU(info);
	if (numIRQs == 0) {
		IOLog("PDAppleAIC: controller reports no interrupt sources (info 0x%x)\n", info);
		return false;
	}

	/*
	 * Mask every source and point them all at CPU 0 before anything can be
	 * registered. Whatever the booter left enabled is not ours to service.
	 */
	for (uint32_t i = 0; i < (numIRQs + 31) / 32; i++) {
		write32(kAICIntMaskSet(i), ~0u);
	}
	for (uint32_t i = 0; i < numIRQs; i++) {
		write32(kAICTargetCPU(i), 1);
	}

	/* Drain anything already latched; reading the event register acknowledges. */
	for (uint32_t i = 0; i < numIRQs; i++) {
		if (kAICEventType(read32(kAICEvent)) == kAICEventTypeSpurious) {
			break;
		}
	}

	vectors = (IOInterruptVector *)IOMalloc(numIRQs * sizeof(IOInterruptVector));
	if (vectors == NULL) {
		return false;
	}
	bzero(vectors, numIRQs * sizeof(IOInterruptVector));

	controllerLock = IOSimpleLockAlloc();
	if (controllerLock == NULL) {
		return false;
	}
	for (uint32_t i = 0; i < numIRQs; i++) {
		vectors[i].interruptLock = IOLockAlloc();
		if (vectors[i].interruptLock == NULL) {
			return false;
		}
	}

	/*
	 * Publish under the name device tree nubs will look for - IODeviceTree
	 * builds "IOInterruptController%08X" from the controller node's phandle -
	 * and as the primary controller, which is the fallback for nubs with no
	 * explicit interrupt parent.
	 */
	getPlatform()->registerInterruptController(
		(OSSymbol *)gIODTDefaultInterruptController, this);

	/*
	 * Take the CPU's external interrupt. IOCPUInterruptController registered
	 * itself under this name from PDArmCPU::startCommon(); its vector for CPU 0
	 * is what sleh_irq ends up calling.
	 */
	IOInterruptController *cpuIC = OSDynamicCast(IOInterruptController,
	    getPlatform()->lookUpInterruptController(gPlatformInterruptControllerName));
	if (cpuIC == NULL) {
		IOLog("PDAppleAIC: no CPU interrupt controller to attach to\n");
		return false;
	}

	IOInterruptHandler handler = OSMemberFunctionCast(IOInterruptHandler,
	    this, &PDAppleAIC::handleInterrupt);
	if (cpuIC->registerInterrupt(this, 0, this, handler, NULL) != kIOReturnSuccess) {
		IOLog("PDAppleAIC: could not register with the CPU interrupt controller\n");
		return false;
	}
	cpuIC->enableInterrupt(this, 0);

	IOLog("PDAppleAIC: up at 0x%llx, %u sources, %u cpus (all masked)\n",
	    (uint64_t)phys, numIRQs, numCPUs);
	return true;
}

IOReturn
PDAppleAIC::handleInterrupt(void * /*refCon*/, IOService * /*nub*/, int /*source*/)
{
	for (;;) {
		uint32_t event = read32(kAICEvent);
		uint32_t type  = kAICEventType(event);

		if (type == kAICEventTypeSpurious) {
			break;
		}
		if (type != kAICEventTypeExtInt) {
			/* IPIs and timer events are not delivered through this path. */
			continue;
		}

		uint32_t irq = kAICEventNum(event);
		if (irq >= numIRQs) {
			continue;
		}

		IOInterruptVector *vector = &vectors[irq];
		vector->interruptActive = 1;

		if (!vector->interruptDisabledSoft && vector->interruptRegistered) {
			vector->handler(vector->target, vector->refCon,
			    vector->nub, vector->source);
		} else {
			vector->interruptDisabledHard = 1;
			disableVectorHard(irq, vector);
		}

		vector->interruptActive = 0;
	}

	return kIOReturnSuccess;
}

bool
PDAppleAIC::vectorCanBeShared(IOInterruptVectorNumber /*vectorNumber*/,
    IOInterruptVector * /*vector*/)
{
	return true;
}

int
PDAppleAIC::getVectorType(IOInterruptVectorNumber /*vectorNumber*/,
    IOInterruptVector * /*vector*/)
{
	return kIOInterruptTypeLevel;
}

void
PDAppleAIC::disableVectorHard(IOInterruptVectorNumber vectorNumber,
    IOInterruptVector * /*vector*/)
{
	write32(kAICIntMaskSet(vectorNumber / 32), 1u << (vectorNumber % 32));
}

void
PDAppleAIC::enableVector(IOInterruptVectorNumber vectorNumber,
    IOInterruptVector * /*vector*/)
{
	write32(kAICIntMaskClr(vectorNumber / 32), 1u << (vectorNumber % 32));
}

bool
PDAppleAIC_init(void)
{
	if (gAIC != NULL) {
		return true;
	}

	PDAppleAIC *aic = new PDAppleAIC;
	if (aic == NULL) {
		return false;
	}
	if (!aic->init() || !aic->initAIC()) {
		aic->release();
		return false;
	}

	gAIC = aic;
	return true;
}
