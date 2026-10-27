// SPDX-License-Identifier: MIT
// the iokit face of a device-tree drm driver: matches a platform nub by compatible and runs its linux probe

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <pexpert/pexpert.h>

#include "pd_fbsd_os.h"
#include "pd_fbsd_attach.h"
#include "pd_fbsd_xnu.h"

#define PDDRM_STR2(x)	#x
#define PDDRM_STR(x)	PDDRM_STR2(x)
#define PDDRM_NAME	PDDRM_STR(PDFBSD_CLASS)

class PDFBSD_CLASS : public IOService
{
	OSDeclareDefaultStructors(PDFBSD_CLASS);

public:
#ifdef PDDRM_OPT_IN_BOOTARG
	IOService *probe(IOService *provider, SInt32 *score) override;
#endif
	bool start(IOService *provider) override;
	void systemWillShutdown(IOOptionBits specifier) override;

private:
	void *fDevice;
};

OSDefineMetaClassAndStructors(PDFBSD_CLASS, IOService);

static IOLock *sAttachLock;

#ifdef PDDRM_OPT_IN_BOOTARG
IOService *
PDFBSD_CLASS::probe(IOService *provider, SInt32 *score)
{
	int on = 0;

	PE_parse_boot_argn(PDDRM_OPT_IN_BOOTARG, &on, sizeof(on));
	if (on == 0) {
		IOLog(PDDRM_NAME ": not attaching, boot with %s=1 to use it\n", PDDRM_OPT_IN_BOOTARG);
		return NULL;
	}
	return IOService::probe(provider, score);
}
#endif

static uint32_t
dt_u32(IORegistryEntry *e, const char *name, uint32_t def)
{
	OSData *d = e != NULL ? OSDynamicCast(OSData, e->getProperty(name)) : NULL;
	return d != NULL && d->getLength() >= 4 ? *(const uint32_t *)d->getBytesNoCopy() : def;
}

// one address or size field as the loader stored it: one cell a u32, two cells a native u64
static uint64_t
dt_field(const uint8_t *p, uint32_t cells)
{
	if (cells == 2)
		return *(const uint64_t *)p;
	return cells == 1 ? *(const uint32_t *)p : 0;
}

// an address on the bus below parent, carried up through every ranges property to the cpu
static bool
dt_translate(IORegistryEntry *bus, uint64_t *addr)
{
	while (bus != NULL) {
		IORegistryEntry *up = bus->getParentEntry(gIODTPlane);
		if (up == NULL)
			return true;
		OSData *r = OSDynamicCast(OSData, bus->getProperty("ranges"));
		if (r == NULL)
			return true;
		if (r->getLength() != 0) {
			uint32_t ca = dt_u32(bus, "#address-cells", 2), cs = dt_u32(bus, "#size-cells", 1);
			uint32_t pa = dt_u32(up, "#address-cells", 2);
			uint32_t tuple = 4 * (ca + pa + cs);
			const uint8_t *p = (const uint8_t *)r->getBytesNoCopy();
			bool hit = false;
			for (uint32_t off = 0; tuple != 0 && off + tuple <= r->getLength(); off += tuple) {
				uint64_t child = dt_field(p + off, ca), parent = dt_field(p + off + 4 * ca, pa);
				uint64_t size = dt_field(p + off + 4 * (ca + pa), cs);
				if (*addr >= child && *addr - child < size) {
					*addr = *addr - child + parent;
					hit = true;
					break;
				}
			}
			if (!hit)
				return false;
		}
		bus = up;
	}
	return true;
}

// the node's reg windows, platform nubs here often carry no IODeviceMemory of their own
static int
dt_reg(IORegistryEntry *node, struct pd_fbsd_platform *pd)
{
	IORegistryEntry *bus = node->getParentEntry(gIODTPlane);
	OSData *reg = OSDynamicCast(OSData, node->getProperty("reg"));
	uint32_t ac = dt_u32(bus, "#address-cells", 2), sc = dt_u32(bus, "#size-cells", 1);
	uint32_t tuple = 4 * (ac + sc);

	if (reg == NULL || tuple == 0)
		return 0;
	const uint8_t *p = (const uint8_t *)reg->getBytesNoCopy();
	for (uint32_t off = 0; off + tuple <= reg->getLength() && pd->nmem < PD_FBSD_MAX_MEM; off += tuple) {
		uint64_t addr = dt_field(p + off, ac), size = dt_field(p + off + 4 * ac, sc);
		if (!dt_translate(bus, &addr))
			continue;
		pd->mem_phys[pd->nmem] = addr;
		pd->mem_size[pd->nmem] = size;
		pd->nmem++;
	}
	return pd->nmem;
}

// a three-cell gic specifier (type, number, flags) becomes the one-word intid the gic controller indexes
// by: spis start at intid 32 and ppis at 16 (arm gic architecture), type 0 is an spi, 1 a ppi
static void
gic_specifiers_to_intids(IOService *nub, OSArray *specs)
{
	OSArray *out;

	if (specs == NULL || specs->getCount() == 0)
		return;
	out = OSArray::withCapacity(specs->getCount());
	for (unsigned int i = 0; out != NULL && i < specs->getCount(); i++) {
		OSData *d = OSDynamicCast(OSData, specs->getObject(i));
		if (d == NULL || d->getLength() != 3 * sizeof(uint32_t) ||
		    ((const uint32_t *)d->getBytesNoCopy())[0] > 1) {
			out->release();
			return;
		}
		const uint32_t *c = (const uint32_t *)d->getBytesNoCopy();
		uint32_t intid = c[1] + (c[0] == 0 ? 32 : 16);
		OSData *n = OSData::withBytes(&intid, sizeof(intid));
		out->setObject(n);
		n->release();
	}
	if (out != NULL) {
		nub->setProperty(gIOInterruptSpecifiersKey, out);
		// a nub resolved before this still points at the three-cell data, whose first word (0) became vector 0
		for (int i = 0; nub->_interruptSources != NULL && i < nub->_numInterruptSources && i < (int)out->getCount(); i++) {
			if (nub->_interruptSources[i].vectorData != NULL)
				nub->_interruptSources[i].vectorData = OSDynamicCast(OSData, out->getObject(i));
		}
		IOLog(PDDRM_NAME ": %u gic specifiers as intids, first %u\n", out->getCount(),
		    *(const uint32_t *)OSDynamicCast(OSData, out->getObject(0))->getBytesNoCopy());
		out->release();
	}
}

bool
PDFBSD_CLASS::start(IOService *provider)
{
	struct pd_fbsd_platform pd;

	setProperty("PDDRMState", "start");
	if (!IOService::start(provider))
		return false;
	// a nub from the platform expert is its own device tree entry
	IORegistryEntry *node = provider->inPlane(gIODTPlane) ? provider : NULL;
	if (node == NULL) {
		IOLog(PDDRM_NAME ": the provider is not in the device tree\n");
		return false;
	}

	// pddrmdelay=<seconds> holds the attach back, so its messages land after the boot log wraps
	int delay = 0;
	if (PE_parse_boot_argn("pddrmdelay", &delay, sizeof(delay)) && delay > 0) {
		setProperty("PDDRMState", "delay");
		IOSleep(delay * 1000);
	}

	bzero(&pd, sizeof(pd));
	pd.nub = provider;
	pd.node = node;
	dt_reg(node, &pd);
	// from the specifiers the platform expert attached: getInterruptType would block until the
	// node's interrupt controller is published, which may never happen
	OSArray *specs = OSDynamicCast(OSArray, provider->getProperty(gIOInterruptSpecifiersKey));
	pd.nirq = specs != NULL ? specs->getCount() : 0;
	gic_specifiers_to_intids(provider, specs);
	OSArray *ctrls = OSDynamicCast(OSArray, provider->getProperty(gIOInterruptControllersKey));
	OSSymbol *ctrl = ctrls != NULL ? OSDynamicCast(OSSymbol, ctrls->getObject(0)) : NULL;
	IOLog(PDDRM_NAME ": interrupt controller %s\n", ctrl != NULL ? ctrl->getCStringNoCopy() : "none");
	IOLog(PDDRM_NAME ": %s, %d register windows, %d interrupts\n",
	    node->getName(gIODTPlane), pd.nmem, pd.nirq);
	for (int i = 0; i < pd.nmem; i++)
		IOLog(PDDRM_NAME ":   reg %d: 0x%llx size 0x%llx\n", i, pd.mem_phys[i], pd.mem_size[i]);

	setProperty("PDDRMState", "init");
	pdos_xnu_init();
	setProperty("PDDRMState", "attaching");
	if (sAttachLock == NULL)
		sAttachLock = IOLockAlloc();
	IOLockLock(sAttachLock);
	fDevice = pd_fbsd_attach_platform_device(&pd);
	IOLockUnlock(sAttachLock);
	setProperty("PDDRMState", fDevice != NULL ? "attached" : "failed");
	if (fDevice == NULL) {
		IOLog(PDDRM_NAME ": no linux driver attached\n");
		return false;
	}
	IOLog(PDDRM_NAME ": attached\n");
	registerService();
	return true;
}

void
PDFBSD_CLASS::systemWillShutdown(IOOptionBits specifier)
{
	IOService::systemWillShutdown(specifier);
}
