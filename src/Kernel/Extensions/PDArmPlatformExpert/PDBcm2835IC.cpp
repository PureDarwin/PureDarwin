/*
 * PDBcm2835IC - the BCM2835 ARMCTRL interrupt controller, as an
 * IOInterruptController.
 *
 */
#include "PDBcm2835IC.h"
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>

/*
 * ARMCTRL lives at peripheral base + 0xB000; the interrupt registers are the
 * 0x200 window inside it. BCM2835 puts the peripherals at 0x20000000 (the
 * BCM2837 on the arm64 side uses 0x3F000000).
 */
#define BCM2835_ARMCTRL_PHYS     0x2000B000
#define BCM283X_ARMCTRL_OFFSET   0x0000B000

/*
 * BCM2835 puts the peripherals at 0x20000000, BCM2837 at 0x3f000000. The
 * loader publishes the correct base as "peripheral-base" on the device tree
 * root, so one binary serves both; absent that, keep the BCM2835 value.
 */
static uint32_t
pd_bcm283x_periph_base(void)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	uint32_t base = BCM2835_ARMCTRL_PHYS - BCM283X_ARMCTRL_OFFSET;

	if (root == NULL) return base;

	OSData *pb = OSDynamicCast(OSData, root->getProperty("peripheral-base"));
	if (pb != NULL && pb->getLength() >= (unsigned)sizeof(uint32_t)) {
		base = *(const uint32_t *)pb->getBytesNoCopy();
	}
	root->release();
	return base;
}
#define BCM2835_ARMCTRL_SIZE     0x1000

#define kIRQBasicPending         0x200
#define kIRQPending1             0x204
#define kIRQPending2             0x208
#define kFIQControl              0x20C
#define kEnableIRQs1             0x210
#define kEnableIRQs2             0x214
#define kEnableBasicIRQs         0x218
#define kDisableIRQs1            0x21C
#define kDisableIRQs2            0x220
#define kDisableBasicIRQs        0x224

/* FIQ_CONTROL: bit 7 enables, bits 6:0 select the source. The source index is
 * the same numbering as the vectors below, so GPU IRQ 3 is System Timer C3. */
#define kFIQEnable               (1u << 7)
#define kFIQSrcSystemTimer3      3

/*
 * Only bits 0-7 of IRQ_BASIC_PENDING are ARM sources. Bits 8/9 summarise the
 * two GPU pending registers but deliberately exclude the shortcut sources, and
 * bits 10-20 are shortcut copies of eleven GPU interrupts that are already
 * visible in those registers. Reading the GPU registers directly covers every
 * source exactly once, so only this mask is used.
 */
#define kBasicPendingArmMask     0x000000FF

/*
 * Vector numbering follows the bank layout, GPU 0-63 first and then the eight
 * ARM sources. That is also the encoding FIQ_CONTROL uses for its source
 * index (0-63 GPU, 64-71 ARM), so a vector number can be written there as-is.
 */
#define kNumGPUIRQs              64
#define kNumBasicIRQs            8
#define kNumVectors              (kNumGPUIRQs + kNumBasicIRQs)

class PDBcm2835IC : public IOInterruptController
{
	OSDeclareDefaultStructors(PDBcm2835IC)

private:
	IOMemoryMap      *ctrlMap;
	volatile uint8_t *ctrl;

	uint32_t
	read32(uint32_t off) const
	{
		return *(volatile uint32_t *)(ctrl + off);
	}
	void
	write32(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(ctrl + off) = val;
	}

	/* Split a vector into its bank register offsets and bit position. */
	static void
	decode(IOInterruptVectorNumber v, uint32_t *enableOff, uint32_t *disableOff,
	    uint32_t *bit)
	{
		if (v < 32) {
			*enableOff = kEnableIRQs1;
			*disableOff = kDisableIRQs1;
			*bit = (uint32_t)v;
		} else if (v < kNumGPUIRQs) {
			*enableOff = kEnableIRQs2;
			*disableOff = kDisableIRQs2;
			*bit = (uint32_t)v - 32;
		} else {
			*enableOff = kEnableBasicIRQs;
			*disableOff = kDisableBasicIRQs;
			*bit = (uint32_t)v - kNumGPUIRQs;
		}
	}

	void dispatchBank(uint32_t pending, IOInterruptVectorNumber base);

public:
	bool     mapRegisters(void);
	void     maskAll(void);
	bool     publish(void);

	IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
	bool     vectorCanBeShared(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	int      getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void     disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void     enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
};

#define super IOInterruptController
OSDefineMetaClassAndStructors(PDBcm2835IC, IOInterruptController);

static PDBcm2835IC *gIC;
static bool gMasked;
static bool gPublished;

bool
PDBcm2835IC::mapRegisters(void)
{
	if (ctrl != NULL) {
		return true;
	}

	uint32_t armctrl_phys = pd_bcm283x_periph_base() + BCM283X_ARMCTRL_OFFSET;

	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		(IOPhysicalAddress)armctrl_phys, BCM2835_ARMCTRL_SIZE,
		kIODirectionOutIn);
	if (desc == NULL) {
		return false;
	}
	ctrlMap = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (ctrlMap == NULL) {
		IOLog("PDBcm2835IC: failed to map ARMCTRL at 0x%x\n", armctrl_phys);
		return false;
	}
	ctrl = (volatile uint8_t *)ctrlMap->getVirtualAddress();
	return true;
}

void
PDBcm2835IC::maskAll(void)
{
	/*
	 * Writing a 1 disables; these registers are write-to-clear-enable, so a
	 * blanket write cannot disturb anything else.
	 */
	write32(kDisableBasicIRQs, ~0u);
	write32(kDisableIRQs1, ~0u);
	write32(kDisableIRQs2, ~0u);

	/*
	 * Route the System Timer's channel 3 match to FIQ, which is the
	 * decrementer the scheduler runs on.
	 *
	 * pe_arm_init_timer already writes this during early boot, but that write
	 * goes through the bootstrap V=P peripheral section, which start.s creates
	 * with CACHE_ATTRINDX_DEFAULT - write-back cacheable rather than device
	 * memory - so it lands in the D-cache instead of the peripheral. This
	 * mapping is kIOMapInhibitCache, so the write takes effect here. Until the
	 * bootstrap mapping is corrected this is what actually starts the clock.
	 *
	 * FIQ delivery is separate from the IRQ enables cleared above; per the
	 * datasheet the FIQ source must have its ordinary IRQ enable clear, which
	 * the blanket disable above guarantees.
	 */
	write32(kFIQControl, kFIQEnable | kFIQSrcSystemTimer3);
}

bool
PDBcm2835IC::publish(void)
{
	vectors = (IOInterruptVector *)IOMalloc(kNumVectors * sizeof(IOInterruptVector));
	if (vectors == NULL) {
		return false;
	}
	bzero(vectors, kNumVectors * sizeof(IOInterruptVector));

	controllerLock = IOSimpleLockAlloc();
	if (controllerLock == NULL) {
		return false;
	}
	for (uint32_t i = 0; i < kNumVectors; i++) {
		vectors[i].interruptLock = IOLockAlloc();
		if (vectors[i].interruptLock == NULL) {
			return false;
		}
	}

	/*
	 * Publish under the name device tree nubs look for - IODeviceTree builds
	 * "IOInterruptController%08X" from the controller node's phandle - and as
	 * the primary controller, the fallback for nubs with no explicit parent.
	 */
	getPlatform()->registerInterruptController(
		(OSSymbol *)gIODTDefaultInterruptController, this);

	/*
	 * Take the CPU's external interrupt. IOCPUInterruptController registered
	 * itself under this name from PDArmCPU::startCommon(); its vector for
	 * CPU 0 is what sleh_irq ends up calling.
	 */
	IOInterruptController *cpuIC = OSDynamicCast(IOInterruptController,
	    getPlatform()->lookUpInterruptController(gPlatformInterruptControllerName));
	if (cpuIC == NULL) {
		IOLog("PDBcm2835IC: no CPU interrupt controller to attach to\n");
		return false;
	}

	IOInterruptHandler handler = OSMemberFunctionCast(IOInterruptHandler,
	    this, &PDBcm2835IC::handleInterrupt);
	if (cpuIC->registerInterrupt(this, 0, this, handler, NULL) != kIOReturnSuccess) {
		IOLog("PDBcm2835IC: could not register with the CPU interrupt controller\n");
		return false;
	}
	cpuIC->enableInterrupt(this, 0);

	IOLog("PDBcm2835IC: up at 0x%x, %u sources (all masked)\n",
	    BCM2835_ARMCTRL_PHYS, kNumVectors);
	return true;
}

void
PDBcm2835IC::dispatchBank(uint32_t pending, IOInterruptVectorNumber base)
{
	while (pending != 0) {
		uint32_t bit = (uint32_t)__builtin_ctz(pending);
		pending &= ~(1u << bit);

		IOInterruptVectorNumber num = base + (IOInterruptVectorNumber)bit;
		IOInterruptVector *vector = &vectors[num];

		vector->interruptActive = 1;

		if (!vector->interruptDisabledSoft && vector->interruptRegistered) {
			vector->handler(vector->target, vector->refCon,
			    vector->nub, vector->source);
		} else {
			/*
			 * Nothing will quiesce the source, and there is no EOI to make
			 * the controller drop it, so masking here is the only way out.
			 */
			vector->interruptDisabledHard = 1;
			disableVectorHard(num, vector);
		}

		vector->interruptActive = 0;
	}
}

IOReturn
PDBcm2835IC::handleInterrupt(void * /*refCon*/, IOService * /*nub*/, int /*source*/)
{
	/*
	 * Masking a source is expected to drop it out of the pending registers,
	 * which is what lets this loop finish. Bound it anyway: if that ever does
	 * not hold, returning with the interrupt still asserted is a livelock we
	 * could not observe, whereas leaving the loop lets the console keep up.
	 */
	for (uint32_t pass = 0; pass < kNumVectors; pass++) {
		/*
		 * Both GPU registers are read every time rather than consulting the
		 * bank-8/9 summary bits first. Those bits only report sources that
		 * have no shortcut of their own: the eleven shortcut GPU interrupts
		 * (7, 9, 10, 18, 19 and 53-57, 62 - among them UART0 and USB) show
		 * up in the shortcut bits instead, so a summary-driven read would
		 * never dispatch them and would spin here forever.
		 */
		uint32_t p1 = read32(kIRQPending1);
		uint32_t p2 = read32(kIRQPending2);
		uint32_t basic = read32(kIRQBasicPending) & kBasicPendingArmMask;

		if ((p1 | p2 | basic) == 0) {
			break;
		}

		if (p1 != 0) {
			dispatchBank(p1, 0);
		}
		if (p2 != 0) {
			dispatchBank(p2, 32);
		}
		if (basic != 0) {
			dispatchBank(basic, kNumGPUIRQs);
		}
	}

	return kIOReturnSuccess;
}

bool
PDBcm2835IC::vectorCanBeShared(IOInterruptVectorNumber /*vectorNumber*/,
    IOInterruptVector * /*vector*/)
{
	return true;
}

int
PDBcm2835IC::getVectorType(IOInterruptVectorNumber /*vectorNumber*/,
    IOInterruptVector * /*vector*/)
{
	return kIOInterruptTypeLevel;
}

void
PDBcm2835IC::disableVectorHard(IOInterruptVectorNumber vectorNumber,
    IOInterruptVector * /*vector*/)
{
	uint32_t enableOff, disableOff, bit;

	if (vectorNumber < 0 || vectorNumber >= kNumVectors) {
		return;
	}
	decode(vectorNumber, &enableOff, &disableOff, &bit);
	write32(disableOff, 1u << bit);
}

void
PDBcm2835IC::enableVector(IOInterruptVectorNumber vectorNumber,
    IOInterruptVector * /*vector*/)
{
	uint32_t enableOff, disableOff, bit;

	if (vectorNumber < 0 || vectorNumber >= kNumVectors) {
		return;
	}
	decode(vectorNumber, &enableOff, &disableOff, &bit);
	write32(enableOff, 1u << bit);
}

static PDBcm2835IC *
bcm2835_ic_get(void)
{
	if (gIC != NULL) {
		return gIC;
	}

	PDBcm2835IC *ic = new PDBcm2835IC;
	if (ic == NULL) {
		return NULL;
	}
	if (!ic->init() || !ic->mapRegisters()) {
		ic->release();
		return NULL;
	}

	gIC = ic;
	return gIC;
}

bool
PDBcm2835IC_maskAll(void)
{
	if (gMasked) {
		return true;
	}

	PDBcm2835IC *ic = bcm2835_ic_get();
	if (ic == NULL) {
		return false;
	}

	ic->maskAll();
	gMasked = true;
	return true;
}

bool
PDBcm2835IC_init(void)
{
	if (gPublished) {
		return true;
	}
	if (!PDBcm2835IC_maskAll()) {
		return false;
	}
	if (!gIC->publish()) {
		return false;
	}

	gPublished = true;
	return true;
}
