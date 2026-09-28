// the risc-v platform-level interrupt controller as an IOInterruptController
// register layout from the risc-v plic specification

#include "PDRiscvPLIC.h"
#include "PDRiscvDT.h"
#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>
#include <riscv/machine_routines.h>

// source priorities, pending bits and per context enable bits share the first window
#define PLIC_PRIORITY(src)		(4u * (src))
#define PLIC_ENABLE(ctx, word)		(0x2000u + 0x80u * (ctx) + 4u * (word))
// each context has a 4 KiB page of threshold and claim/complete from here on
#define PLIC_CONTEXT_BASE		0x200000u
#define PLIC_CONTEXT_STRIDE		0x1000u
#define PLIC_THRESHOLD			0x0u
#define PLIC_CLAIM			0x4u

// source 0 means no interrupt, 1023 is the most the spec allows
#define PLIC_MAX_SOURCES		1024u
#define PLIC_MAX_CONTEXTS		15872u

// the supervisor external interrupt number in a hart's local interrupt space
#define RISCV_IRQ_S_EXT			9u

#define PD_PLIC_MAX_CPUS		64u

static const char * const gPLICCompatibles[] = {
	"sifive,plic-1.0.0",
	"riscv,plic0",
	// the c906 and c910 variant, the t-head extra control register stays with the firmware
	"thead,c900-plic",
};

class PDRiscvPLIC : public IOInterruptController
{
	OSDeclareDefaultStructors(PDRiscvPLIC)

private:
	IORegistryEntry  *plicNode;
	IOMemoryMap      *sourceMap;
	IOMemoryMap      *contextMap;
	volatile uint8_t *sourceRegs;
	volatile uint8_t *contextRegs;
	uint64_t          plicPhys;
	uint32_t          numSources;
	uint32_t          numContexts;
	uint32_t          cpuCount;
	uint32_t          cpuContext[PD_PLIC_MAX_CPUS];
	uint32_t          targetContext;

	uint32_t
	srcRead(uint32_t off) const
	{
		return *(volatile uint32_t *)(sourceRegs + off);
	}
	void
	srcWrite(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(sourceRegs + off) = val;
	}
	volatile uint32_t *
	contextReg(uint32_t ctx, uint32_t off) const
	{
		return (volatile uint32_t *)(contextRegs + ctx * PLIC_CONTEXT_STRIDE + off);
	}

	bool findContexts(void);
	bool mapRegisters(uint64_t size);
	void quietContext(uint32_t ctx);

public:
	bool     initPLIC(void);
	bool     startPLIC(void);

	IOReturn registerInterrupt(IOService *nub, int source, void *target,
	    IOInterruptHandler handler, void *refCon) APPLE_KEXT_OVERRIDE;
	IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
	bool     vectorCanBeShared(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	int      getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void     disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
	void     enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
};

#define super IOInterruptController
OSDefineMetaClassAndStructors(PDRiscvPLIC, IOInterruptController);

static PDRiscvPLIC *gPLIC;

static volatile uint8_t *
map_phys(uint64_t phys, uint64_t size, IOMemoryMap **outMap)
{
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		(IOPhysicalAddress)phys, (IOByteCount)size, kIODirectionOutIn);
	IOMemoryMap *map;

	if (desc == NULL) {
		return NULL;
	}
	map = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (map == NULL) {
		return NULL;
	}
	*outMap = map;
	return (volatile uint8_t *)map->getVirtualAddress();
}

// the hart behind an interrupts-extended phandle, which names a cpu's riscv,cpu-intc child
// or the cpu node itself, plus that node's #interrupt-cells
static bool
hart_for_phandle(uint32_t phandle, uint32_t *hart, uint32_t *icells)
{
	IORegistryEntry *cpus = IORegistryEntry::fromPath("/cpus", gIODTPlane);
	OSIterator *iter;
	bool found = false;

	if (cpus == NULL) {
		return false;
	}
	iter = cpus->getChildIterator(gIODTPlane);
	while (iter != NULL && !found) {
		IORegistryEntry *cpu = OSDynamicCast(IORegistryEntry, iter->getNextObject());
		uint64_t reg;
		uint32_t ph;

		if (cpu == NULL) {
			break;
		}
		if (!PDRiscvDT_getCell(cpu, "reg", &reg)) {
			continue;
		}
		if (PDRiscvDT_getPHandle(cpu, &ph) && ph == phandle) {
			*hart = (uint32_t)reg;
			if (!PDRiscvDT_getU32(cpu, "#interrupt-cells", icells)) {
				*icells = 1;
			}
			found = true;
			break;
		}

		OSIterator *kids = cpu->getChildIterator(gIODTPlane);
		while (kids != NULL) {
			IORegistryEntry *intc = OSDynamicCast(IORegistryEntry, kids->getNextObject());

			if (intc == NULL) {
				break;
			}
			if (PDRiscvDT_getPHandle(intc, &ph) && ph == phandle) {
				*hart = (uint32_t)reg;
				if (!PDRiscvDT_getU32(intc, "#interrupt-cells", icells)) {
					*icells = 1;
				}
				found = true;
				break;
			}
		}
		OSSafeReleaseNULL(kids);
	}
	OSSafeReleaseNULL(iter);
	cpus->release();
	return found;
}

// the context index is the position of the hart's s-mode entry in interrupts-extended
static bool
context_from_dt(IORegistryEntry *plic, uint32_t hart, uint32_t *ctx)
{
	OSData *ie = OSDynamicCast(OSData, plic->getProperty("interrupts-extended"));

	if (ie == NULL) {
		return false;
	}

	const uint32_t *cells = (const uint32_t *)ie->getBytesNoCopy();
	uint32_t count = ie->getLength() / sizeof(uint32_t);
	uint32_t index = 0;

	for (uint32_t pos = 0; pos < count; index++) {
		uint32_t h, icells = 1;
		bool known = hart_for_phandle(cells[pos], &h, &icells);

		if (icells == 0 || pos + 1 + icells > count) {
			break;
		}
		if (known && h == hart && cells[pos + 1] == RISCV_IRQ_S_EXT) {
			*ctx = index;
			return true;
		}
		pos += 1 + icells;
	}
	return false;
}

bool
PDRiscvPLIC::findContexts(void)
{
	const ml_topology_info_t *topo = ml_get_topology_info();
	uint32_t bootCpu = 0;

	cpuCount = (topo != NULL) ? topo->num_cpus : 1;
	if (cpuCount > PD_PLIC_MAX_CPUS) {
		IOLog("PDRiscvPLIC: %u cpus, only the first %u get a context\n", cpuCount, PD_PLIC_MAX_CPUS);
		cpuCount = PD_PLIC_MAX_CPUS;
	}

	numContexts = 0;
	for (uint32_t i = 0; i < cpuCount; i++) {
		uint32_t hart = (topo != NULL) ? topo->cpus[i].phys_id : 0;
		uint32_t logical = (topo != NULL) ? topo->cpus[i].cpu_id : 0;
		uint32_t ctx;

		if (logical >= PD_PLIC_MAX_CPUS) {
			continue;
		}
		if (!context_from_dt(plicNode, hart, &ctx)) {
			// qemu virt and most sifive-style parts pair an m-mode and an s-mode context per hart
			ctx = 2 * hart + 1;
			IOLog("PDRiscvPLIC: no interrupts-extended entry for hart %u, guessing context %u\n",
			    hart, ctx);
		}
		if (ctx >= PLIC_MAX_CONTEXTS) {
			IOLog("PDRiscvPLIC: context %u for hart %u is out of range\n", ctx, hart);
			return false;
		}
		cpuContext[logical] = ctx;
		if (ctx + 1 > numContexts) {
			numContexts = ctx + 1;
		}
	}

	if (topo != NULL && topo->boot_cpu != NULL) {
		bootCpu = topo->boot_cpu->cpu_id;
	}
	// device interrupts go to the boot hart, any hart may still claim through its own context
	targetContext = cpuContext[bootCpu < PD_PLIC_MAX_CPUS ? bootCpu : 0];
	return numContexts != 0;
}

bool
PDRiscvPLIC::mapRegisters(uint64_t size)
{
	uint64_t srcLen = round_page(PLIC_ENABLE(numContexts, 0));
	uint64_t ctxLen = round_page((uint64_t)numContexts * PLIC_CONTEXT_STRIDE);

	if (size != 0 && PLIC_CONTEXT_BASE + ctxLen > size) {
		IOLog("PDRiscvPLIC: %u contexts overrun the 0x%llx byte register window\n",
		    numContexts, (unsigned long long)size);
		return false;
	}

	sourceRegs = map_phys(plicPhys, srcLen, &sourceMap);
	contextRegs = map_phys(plicPhys + PLIC_CONTEXT_BASE, ctxLen, &contextMap);
	if (sourceRegs == NULL || contextRegs == NULL) {
		IOLog("PDRiscvPLIC: failed to map registers at 0x%llx\n", (unsigned long long)plicPhys);
		return false;
	}
	return true;
}

// mask every source in one context and let any priority through once one is enabled
void
PDRiscvPLIC::quietContext(uint32_t ctx)
{
	for (uint32_t word = 0; word < (numSources + 31) / 32; word++) {
		srcWrite(PLIC_ENABLE(ctx, word), 0);
	}
	*contextReg(ctx, PLIC_THRESHOLD) = 0;
}

bool
PDRiscvPLIC::initPLIC(void)
{
	uint64_t size = 0;
	uint32_t ndev = 0;

	plicNode = PDRiscvDT_findCompatible(gPLICCompatibles,
	    sizeof(gPLICCompatibles) / sizeof(gPLICCompatibles[0]));
	if (plicNode == NULL) {
		IOLog("PDRiscvPLIC: no plic in the device tree\n");
		return false;
	}
	if (!PDRiscvDT_getReg(plicNode, 0, &plicPhys, &size) || plicPhys == 0) {
		IOLog("PDRiscvPLIC: plic has no usable reg property\n");
		return false;
	}

	if (!PDRiscvDT_getU32(plicNode, "riscv,ndev", &ndev) || ndev == 0 || ndev >= PLIC_MAX_SOURCES) {
		IOLog("PDRiscvPLIC: no usable riscv,ndev, assuming %u sources\n", PLIC_MAX_SOURCES - 1);
		ndev = PLIC_MAX_SOURCES - 1;
	}
	numSources = ndev + 1;

	if (!findContexts() || !mapRegisters(size)) {
		return false;
	}

	// nothing may reach a hart before a handler exists for it, the kernel panics on a stray sei
	for (uint32_t i = 0; i < cpuCount; i++) {
		quietContext(cpuContext[i]);
	}
	for (uint32_t src = 1; src < numSources; src++) {
		srcWrite(PLIC_PRIORITY(src), 0);
	}

	vectors = (IOInterruptVector *)IOMalloc(numSources * sizeof(IOInterruptVector));
	if (vectors == NULL) {
		return false;
	}
	bzero(vectors, numSources * sizeof(IOInterruptVector));

	controllerLock = IOSimpleLockAlloc();
	if (controllerLock == NULL) {
		return false;
	}
	for (uint32_t i = 0; i < numSources; i++) {
		vectors[i].interruptLock = IOLockAlloc();
		if (vectors[i].interruptLock == NULL) {
			return false;
		}
	}

	return true;
}

bool
PDRiscvPLIC::startPLIC(void)
{
	IOPlatformExpert *platform = getPlatform();
	uint32_t phandle;

	if (platform == NULL) {
		return false;
	}
	attach(platform);

	// device tree nubs look for the controller named after its phandle, the rest fall back to the primary
	if (PDRiscvDT_getPHandle(plicNode, &phandle)) {
		char name[48];

		snprintf(name, sizeof(name), "IOInterruptController%08X", phandle);
		const OSSymbol *sym = OSSymbol::withCString(name);
		if (sym != NULL) {
			platform->registerInterruptController((OSSymbol *)sym, this);
			sym->release();
		}
	} else {
		IOLog("PDRiscvPLIC: plic has no phandle, devices reach it only as the primary controller\n");
	}
	platform->registerInterruptController((OSSymbol *)gIODTDefaultInterruptController, this);

	// every hart's sei arrives through the cpu interrupt controller, its source is the cpu number
	IOInterruptController *cpuIC = OSDynamicCast(IOInterruptController,
	    platform->lookUpInterruptController(gPlatformInterruptControllerName));
	if (cpuIC == NULL) {
		IOLog("PDRiscvPLIC: no cpu interrupt controller to attach to\n");
		return false;
	}

	IOInterruptHandler handler = OSMemberFunctionCast(IOInterruptHandler,
	    this, &PDRiscvPLIC::handleInterrupt);
	if (cpuIC->registerInterrupt(this, 0, this, handler, NULL) != kIOReturnSuccess) {
		IOLog("PDRiscvPLIC: could not register with the cpu interrupt controller\n");
		return false;
	}
	cpuIC->enableInterrupt(this, 0);
	return true;
}

// the base class indexes its vector table with the first specifier cell unchecked
IOReturn
PDRiscvPLIC::registerInterrupt(IOService *nub, int source, void *target,
    IOInterruptHandler handler, void *refCon)
{
	OSArray *specs = OSDynamicCast(OSArray, nub->getProperty(gIOInterruptSpecifiersKey));
	OSData *spec = (specs != NULL) ? OSDynamicCast(OSData, specs->getObject((unsigned int)source)) : NULL;

	if (spec == NULL || spec->getLength() < sizeof(uint32_t)) {
		return kIOReturnNoInterrupt;
	}

	uint32_t id = *(const uint32_t *)spec->getBytesNoCopy();
	if (id == 0 || id >= numSources) {
		IOLog("PDRiscvPLIC: %s asked for source %u of %u\n", nub->getName(), id, numSources - 1);
		return kIOReturnNoInterrupt;
	}
	return super::registerInterrupt(nub, source, target, handler, refCon);
}

IOReturn
PDRiscvPLIC::handleInterrupt(void * /*refCon*/, IOService * /*nub*/, int source)
{
	uint32_t cpu = (uint32_t)source;

	if (cpu >= cpuCount) {
		return kIOReturnInvalid;
	}

	volatile uint32_t *claim = contextReg(cpuContext[cpu], PLIC_CLAIM);

	// a claim hands out the highest priority pending source and clears its pending bit
	for (;;) {
		uint32_t id = *claim;

		if (id == 0) {
			break;
		}
		if (id < numSources) {
			IOInterruptVector *vector = &vectors[id];

			vector->interruptActive = 1;
			if (!vector->interruptDisabledSoft && vector->interruptRegistered) {
				vector->handler(vector->target, vector->refCon,
				    vector->nub, vector->source);
			} else {
				vector->interruptDisabledHard = 1;
				disableVectorHard(id, vector);
			}
			vector->interruptActive = 0;
		}
		// completing reopens the gateway for the next request from that source
		*claim = id;
	}

	return kIOReturnSuccess;
}

bool
PDRiscvPLIC::vectorCanBeShared(IOInterruptVectorNumber /*vectorNumber*/,
    IOInterruptVector * /*vector*/)
{
	return true;
}

int
PDRiscvPLIC::getVectorType(IOInterruptVectorNumber /*vectorNumber*/,
    IOInterruptVector * /*vector*/)
{
	return kIOInterruptTypeLevel;
}

void
PDRiscvPLIC::disableVectorHard(IOInterruptVectorNumber vectorNumber,
    IOInterruptVector * /*vector*/)
{
	uint32_t n = (uint32_t)vectorNumber;
	uint32_t off = PLIC_ENABLE(targetContext, n / 32);
	IOInterruptState is;

	if (n == 0 || n >= numSources) {
		return;
	}
	is = IOSimpleLockLockDisableInterrupt(controllerLock);
	srcWrite(off, srcRead(off) & ~(1u << (n % 32)));
	IOSimpleLockUnlockEnableInterrupt(controllerLock, is);
}

void
PDRiscvPLIC::enableVector(IOInterruptVectorNumber vectorNumber,
    IOInterruptVector * /*vector*/)
{
	uint32_t n = (uint32_t)vectorNumber;
	uint32_t off = PLIC_ENABLE(targetContext, n / 32);
	IOInterruptState is;

	if (n == 0 || n >= numSources) {
		return;
	}
	is = IOSimpleLockLockDisableInterrupt(controllerLock);
	// every source shares priority 1, above the threshold of 0
	srcWrite(PLIC_PRIORITY(n), 1);
	srcWrite(off, srcRead(off) | (1u << (n % 32)));
	IOSimpleLockUnlockEnableInterrupt(controllerLock, is);
}

bool
PDRiscvPLIC_init(void)
{
	PDRiscvPLIC *plic;

	if (gPLIC != NULL) {
		return true;
	}
	plic = new PDRiscvPLIC;
	if (plic == NULL) {
		return false;
	}
	if (!plic->init() || !plic->initPLIC()) {
		plic->release();
		return false;
	}
	gPLIC = plic;
	return true;
}

bool
PDRiscvPLIC_start(void)
{
	if (gPLIC == NULL) {
		return false;
	}
	return gPLIC->startPLIC();
}
