#include "PDArmGICController.h"
#include "PDArmGIC.h"

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOService.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <pexpert/pexpert.h>
#include <kern/clock.h>

#if defined(__arm64__)

#define GICD_TYPER		0x0004
#define GICD_ISENABLER		0x0100
#define GICD_ICENABLER		0x0180
#define GICD_IPRIORITYR		0x0400
#define GICD_IROUTER		0x6000
#define GICD_IGROUPR		0x0080
#define GICD_ITARGETSR		0x0800
#define kSPIPriority		0xa0

extern "C" uint32_t pd_gic_current_spi(void);

class PDArmGICController : public IOInterruptController
{
	OSDeclareDefaultStructors(PDArmGICController)

public:
	bool initGIC(volatile uint8_t *gicd, bool v2);
	static void attachThread(void *arg, wait_result_t wr);
	static const OSSymbol *rootInterruptParentName(void);
	IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
	bool vectorCanBeShared(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	int getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;

private:
	volatile uint8_t *fGicd;
	uint32_t          fLines;
	uint64_t          fRoute;
	// GICv2: spis are Group 1 (irq) and go to this cpu interface through ITARGETSR, not IROUTER
	bool              fV2;
	// /arm-io/gic's phandle name, the interrupt parent device tree nubs list, when the loader has one
	const OSSymbol   *fDTName;

	void dwrite(uint32_t off, uint32_t val) { *(volatile uint32_t *)(fGicd + off) = val; }
	uint32_t dread(uint32_t off) { return *(volatile uint32_t *)(fGicd + off); }

public:
	void watchLine(uint32_t v);
};

OSDefineMetaClassAndStructors(PDArmGICController, IOInterruptController);

static PDArmGICController *gGICController;
static thread_call_t gGICWatchCall;
static uint32_t gGICWatchLine, gGICWatchCount;

// distributor state of one line, read only: enable, pending, active, group, config, target and priority
void
PDArmGICController::watchLine(uint32_t v)
{
	uint32_t w = (v / 32) * 4, b = 1u << (v % 32);

	IOLog("PDArmGIC: watch %u ctlr 0x%x en %d pend %d act %d grp %d cfg 0x%x target 0x%x prio 0x%x handled %d\n", v,
	    dread(0), (dread(GICD_ISENABLER + w) & b) != 0, (dread(0x200 + w) & b) != 0, (dread(0x300 + w) & b) != 0,
	    (dread(GICD_IGROUPR + w) & b) != 0, (dread(0xc00 + (v / 16) * 4) >> ((v % 16) * 2)) & 3,
	    fV2 ? fGicd[GICD_ITARGETSR + v] : 0, fGicd[GICD_IPRIORITYR + v], vectors[v].interruptRegistered);
}

static void
gicWatch(thread_call_param_t, thread_call_param_t)
{
	uint64_t next;

	if (gGICController == NULL || gGICWatchCount++ >= 60)
		return;
	gGICController->watchLine(gGICWatchLine);
	clock_interval_to_deadline(5, kSecondScale, &next);
	thread_call_enter_delayed(gGICWatchCall, next);
}

// the controller name of the node the root's interrupt-parent phandle points at
const OSSymbol *
PDArmGICController::rootInterruptParentName(void)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	OSData *ip = root != NULL ? OSDynamicCast(OSData, root->getProperty("interrupt-parent")) : NULL;
	const OSSymbol *name = NULL;

	if (ip != NULL && ip->getLength() == 4) {
		uint32_t ph = *(const uint32_t *)ip->getBytesNoCopy();
		IORegistryIterator *it = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
		IORegistryEntry *e;

		while (it != NULL && name == NULL && (e = it->getNextObject()) != NULL) {
			OSData *d = OSDynamicCast(OSData, e->getProperty("AAPL,phandle"));
			if (d != NULL && d->getLength() == 4 && *(const uint32_t *)d->getBytesNoCopy() == ph &&
			    e->getProperty("interrupt-controller") != NULL)
				name = IODTInterruptControllerName(e);
		}
		OSSafeReleaseNULL(it);
	}
	OSSafeReleaseNULL(root);
	return name;
}

bool
PDArmGICController::initGIC(volatile uint8_t *gicd, bool v2)
{
	uint64_t mpidr;

	fGicd = gicd;
	fV2 = v2;
	fLines = ((dread(GICD_TYPER) & 0x1f) + 1) * 32;
	if (fLines > 1020)
		fLines = 1020;
	// every spi goes to the boot cpu, the one running this
	__asm__ volatile ("mrs %0, MPIDR_EL1" : "=r"(mpidr));
	fRoute = ((mpidr >> 32) & 0xff) << 32 | (mpidr & 0xffffff);
	// ITARGETSR0 reads back the calling cpu's own interface bit. every spi is Group 1 from the start, as a per-vector
	// read-modify-write races drivers enabling neighbours and a Group 0 spi is an fiq the timer path swallows
	if (v2) {
		fRoute = fGicd[GICD_ITARGETSR];
		for (uint32_t w = 1; w < fLines / 32; w++)
			dwrite(GICD_IGROUPR + w * 4, 0xffffffffu);
	}

	vectors = (IOInterruptVector *)IOMalloc(fLines * sizeof(IOInterruptVector));
	if (vectors == NULL)
		return false;
	bzero(vectors, fLines * sizeof(IOInterruptVector));
	for (uint32_t i = 0; i < fLines; i++) {
		vectors[i].interruptLock = IOLockAlloc();
		if (vectors[i].interruptLock == NULL)
			return false;
	}

	getPlatform()->registerInterruptController((OSSymbol *)OSSymbol::withCStringNoCopy("PDArmGIC"), this);
	IORegistryEntry *gic = IORegistryEntry::fromPath("/arm-io/gic", gIODTPlane);
	if (gic != NULL) {
		// only a device tree that gives the gic a phandle names it, qemu virt's has none
		if (gic->getProperty("AAPL,phandle") != NULL)
			fDTName = IODTInterruptControllerName(gic);
		if (fDTName != NULL)
			getPlatform()->registerInterruptController((OSSymbol *)fDTName, this);
		gic->release();
	}
	// an l4t style tree names the gic through the root's interrupt-parent instead
	if (fDTName == NULL && (fDTName = rootInterruptParentName()) != NULL)
		getPlatform()->registerInterruptController((OSSymbol *)fDTName, this);

	// registering with the cpu controller blocks until every cpu takes interrupts, which only
	// happens after the platform expert's start returns
	thread_t th = NULL;

	retain();
	if (kernel_thread_start(&PDArmGICController::attachThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	IOLog("PDArmGIC: spi controller registered, %u lines, routed to 0x%llx\n", fLines, fRoute);
	return true;
}

void
PDArmGICController::attachThread(void *arg, wait_result_t)
{
	PDArmGICController *self = (PDArmGICController *)arg;
	IOInterruptController *cpuIC = OSDynamicCast(IOInterruptController,
	    getPlatform()->lookUpInterruptController(gPlatformInterruptControllerName));
	IOInterruptHandler handler = OSMemberFunctionCast(IOInterruptHandler, self,
	    &PDArmGICController::handleInterrupt);

	if (cpuIC == NULL) {
		IOLog("PDArmGIC: no cpu interrupt controller to take spis from\n");
	} else if (cpuIC->registerInterrupt(self, 0, self, handler, NULL) != kIOReturnSuccess) {
		// the vector is live before this returns, and the return waits for every cpu
		IOLog("PDArmGIC: could not register with the cpu interrupt controller\n");
	} else {
		cpuIC->enableInterrupt(self, 0);
		// drivers of device tree nubs wait for this, with a deadline, before registering
		if (self->fDTName != NULL)
			self->publishResource(self->fDTName, self);
		IOLog("PDArmGIC: spi controller up, %u lines, routed to 0x%llx\n", self->fLines, self->fRoute);
		// pdgicwatch=N prints line N's distributor state every 5 s for 5 minutes
		if (PE_parse_boot_argn("pdgicwatch", &gGICWatchLine, sizeof(gGICWatchLine)) && gGICWatchLine >= 32 &&
		    gGICWatchLine < self->fLines && (gGICWatchCall = thread_call_allocate(gicWatch, NULL)) != NULL)
			thread_call_enter(gGICWatchCall);
	}
	self->release();
	thread_terminate(current_thread());
}

IOReturn
PDArmGICController::handleInterrupt(void *, IOService *, int)
{
	uint32_t intid = pd_gic_current_spi();
	IOInterruptVector *vector;

	if (intid < 32 || intid >= fLines)
		return kIOReturnSuccess;
	vector = &vectors[intid];
	vector->interruptActive = 1;
	if (!vector->interruptDisabledSoft && vector->interruptRegistered) {
		vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
		// a level source the handler disabled until its work loop runs would fire again on the eoi
		if (vector->interruptDisabledSoft) {
			vector->interruptDisabledHard = 1;
			disableVectorHard(intid, vector);
		}
	} else {
		vector->interruptDisabledHard = 1;
		disableVectorHard(intid, vector);
	}
	vector->interruptActive = 0;
	return kIOReturnSuccess;
}

bool
PDArmGICController::vectorCanBeShared(IOInterruptVectorNumber, IOInterruptVector *)
{
	return true;
}

int
PDArmGICController::getVectorType(IOInterruptVectorNumber, IOInterruptVector *)
{
	return kIOInterruptTypeLevel;
}

void
PDArmGICController::disableVectorHard(IOInterruptVectorNumber v, IOInterruptVector *)
{
	if (v < 32 || v >= fLines)
		return;
	dwrite(GICD_ICENABLER + (v / 32) * 4, 1u << (v % 32));
}

void
PDArmGICController::enableVector(IOInterruptVectorNumber v, IOInterruptVector *)
{
	if (v < 32 || v >= fLines)
		return;
	fGicd[GICD_IPRIORITYR + v] = kSPIPriority;
	if (fV2) {
		fGicd[GICD_ITARGETSR + v] = (uint8_t)fRoute;
	} else {
		*(volatile uint64_t *)(fGicd + GICD_IROUTER + v * 8) = fRoute;
	}
	__asm__ volatile ("dsb sy" ::: "memory");
	dwrite(GICD_ISENABLER + (v / 32) * 4, 1u << (v % 32));
}

bool
PDArmGICController_init(void)
{
	volatile uint8_t *gicd = PDArmGIC_v3_distributor();
	bool v2 = false;
	PDArmGICController *ic;

	if (gGICController != NULL)
		return true;
	if (gicd == NULL) {
		gicd = PDArmGIC_v2_distributor();
		v2 = true;
	}
	if (gicd == NULL)
		return false;
	ic = new PDArmGICController;
	if (ic == NULL)
		return false;
	if (!ic->init() || !ic->initGIC(gicd, v2)) {
		ic->release();
		return false;
	}
	gGICController = ic;
	return true;
}

#else

bool
PDArmGICController_init(void)
{
	return false;
}

#endif
