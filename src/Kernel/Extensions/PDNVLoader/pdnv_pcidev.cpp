// pdnv_pcidev.cpp - the GPU's PCI side: config space, physical mappings, and the driver that hands a device to the resource manager
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "pdnv_pci.h"

#define PDNV_RM_WAIT_NS     (180ull * 1000 * 1000 * 1000)

// config space

extern "C" int
pdnv_cfg_read(void *handle, uint32_t off, int width, uint32_t *v)
{
	IOPCIDevice *dev = OSDynamicCast(IOPCIDevice, (OSObject *)handle);

	if (dev == NULL || off >= 0x1000 || (off & (uint32_t)(width - 1)) != 0)
		return -1;
	switch (width) {
	case 1:
		*v = dev->extendedConfigRead8(off);
		break;
	case 2:
		*v = dev->extendedConfigRead16(off);
		break;
	default:
		*v = dev->extendedConfigRead32(off);
		break;
	}
	return 0;
}

extern "C" int
pdnv_cfg_write(void *handle, uint32_t off, int width, uint32_t v)
{
	IOPCIDevice *dev = OSDynamicCast(IOPCIDevice, (OSObject *)handle);

	if (dev == NULL || off >= 0x1000 || (off & (uint32_t)(width - 1)) != 0)
		return -1;
	switch (width) {
	case 1:
		dev->extendedConfigWrite8(off, (UInt8)v);
		break;
	case 2:
		dev->extendedConfigWrite16(off, (UInt16)v);
		break;
	default:
		dev->extendedConfigWrite32(off, v);
		break;
	}
	return 0;
}

// any device by its address, as the resource manager asks for bridges and its own functions
extern "C" void *
pdnv_pci_find(uint32_t domain, uint8_t bus, uint8_t slot, uint8_t func, uint16_t *vendor, uint16_t *device)
{
	OSIterator *it = IOService::getMatchingServices(IOService::serviceMatching("IOPCIDevice"));
	IOPCIDevice *found = NULL;
	OSObject *o;

	if (it == NULL)
		return NULL;
	while ((o = it->getNextObject()) != NULL) {
		IOPCIDevice *dev = OSDynamicCast(IOPCIDevice, o);

		if (dev != NULL && domain == 0 && dev->getBusNumber() == bus && dev->getDeviceNumber() == slot &&
		    dev->getFunctionNumber() == func) {
			found = dev;
			break;
		}
	}
	it->release();
	// IOPCIDevices live as long as the slot, the handle is not retained, as on Linux
	if (found != NULL) {
		if (vendor != NULL)
			*vendor = found->configRead16(kIOPCIConfigVendorID);
		if (device != NULL)
			*device = found->configRead16(kIOPCIConfigDeviceID);
	}
	return found;
}

// physical mappings, remembered by their address so the unmap can find them

struct pdnv_map {
	struct pdnv_map *next;
	IOMemoryMap *map;
	void *va;
};

static struct pdnv_map *sMaps;
static IOLock *sMapLock;

extern "C" void
pdnv_pci_init(void)
{
	if (sMapLock == NULL)
		sMapLock = IOLockAlloc();
}

extern "C" void *
pdnv_map_phys(uint64_t pa, uint64_t size, int cache)
{
	uint64_t base = pa & ~(uint64_t)PAGE_MASK;
	uint64_t len = round_page(pa + size) - base;
	IOOptionBits opt = kIOMapAnywhere | (cache == PDNV_MAP_CACHED ? kIOMapDefaultCache :
	    cache == PDNV_MAP_WC ? kIOMapWriteCombineCache : kIOMapInhibitCache);
	IOMemoryDescriptor *md;
	IOMemoryMap *map;
	struct pdnv_map *m;

	if (size == 0 || sMapLock == NULL)
		return NULL;
	md = IOMemoryDescriptor::withAddressRange(base, len, kIODirectionInOut | kIOMemoryMapperNone, NULL);
	if (md == NULL)
		return NULL;
	map = md->createMappingInTask(kernel_task, 0, opt);
	md->release();
	if (map == NULL)
		return NULL;
	m = IOMallocType(struct pdnv_map);
	m->map = map;
	m->va = (void *)(map->getVirtualAddress() + (pa - base));
	IOLockLock(sMapLock);
	m->next = sMaps;
	sMaps = m;
	IOLockUnlock(sMapLock);
	return m->va;
}

extern "C" void
pdnv_unmap_phys(void *va)
{
	struct pdnv_map **pp, *m = NULL;

	if (sMapLock == NULL)
		return;
	IOLockLock(sMapLock);
	for (pp = &sMaps; *pp != NULL; pp = &(*pp)->next) {
		if ((*pp)->va == va) {
			m = *pp;
			*pp = m->next;
			break;
		}
	}
	IOLockUnlock(sMapLock);
	if (m == NULL)
		return;
	m->map->release();
	IOFreeType(m, struct pdnv_map);
}

// the driver: NVIDIA display devices, once the resource manager is up

class PDNVGPU : public IOService {
	OSDeclareDefaultStructors(PDNVGPU);

public:
	bool start(IOService *provider) override;

private:
	static bool filter(OSObject *owner, IOFilterInterruptEventSource *src);
	static void action(OSObject *owner, IOInterruptEventSource *src, int count);

	void *fGPU;
	IOWorkLoop *fWorkLoop;
	IOFilterInterruptEventSource *fInterrupt;
};

OSDefineMetaClassAndStructors(PDNVGPU, IOService);

// a memory BAR's address and size from config space, decoding off while the all-ones probe is in
static bool
sizeBar(IOPCIDevice *dev, uint32_t reg, uint32_t raw, struct pdnv_pci_bar *bar)
{
	bool is64 = (raw & 6) == 4;
	uint32_t hi = is64 ? dev->configRead32(reg + 4) : 0;
	uint16_t cmd = dev->configRead16(kIOPCIConfigCommand);
	uint64_t addr = ((uint64_t)hi << 32) | (raw & ~0xfu);
	uint64_t mask;

	if (addr == 0)
		return false;
	dev->configWrite16(kIOPCIConfigCommand, cmd & ~kIOPCICommandMemorySpace);
	dev->configWrite32(reg, 0xffffffff);
	mask = dev->configRead32(reg) & ~0xfull;
	dev->configWrite32(reg, raw);
	if (is64) {
		dev->configWrite32(reg + 4, 0xffffffff);
		mask |= (uint64_t)dev->configRead32(reg + 4) << 32;
		dev->configWrite32(reg + 4, hi);
	} else {
		mask |= 0xffffffff00000000ull;
	}
	dev->configWrite16(kIOPCIConfigCommand, cmd);
	if (mask == 0)
		return false;
	bar->offset = reg;
	bar->cpu_address = addr;
	bar->size = ~mask + 1;
	return true;
}

bool
PDNVGPU::start(IOService *provider)
{
	IOPCIDevice *dev = OSDynamicCast(IOPCIDevice, provider);
	struct pdnv_pci_dev info = {};
	IOService *rm;
	int n = 0;

	if (dev == NULL || !IOService::start(provider))
		return false;

	OSDictionary *match = IOService::resourceMatching(PDNV_RM_RESOURCE);
	rm = IOService::waitForMatchingService(match, PDNV_RM_WAIT_NS);
	match->release();
	if (rm == NULL) {
		IOLog("PDNVGPU: the resource manager is not up, leaving the device\n");
		return false;
	}
	rm->release();

	info.handle = dev;
	info.vendor = dev->configRead16(kIOPCIConfigVendorID);
	info.device = dev->configRead16(kIOPCIConfigDeviceID);
	info.subsystem_vendor = dev->configRead16(kIOPCIConfigSubSystemVendorID);
	info.subsystem_id = dev->configRead16(kIOPCIConfigSubSystemID);
	info.bus = dev->getBusNumber();
	info.slot = dev->getDeviceNumber();
	info.func = dev->getFunctionNumber();
	info.irq = dev->configRead8(kIOPCIConfigInterruptLine);

	// memory BARs in order, as the Linux layer lists them: registers first, then framebuffer and instance memory
	for (uint32_t reg = kIOPCIConfigBaseAddress0; reg <= kIOPCIConfigBaseAddress5 && n < PDNV_MAX_BARS; reg += 4) {
		uint32_t raw = dev->configRead32(reg);
		IODeviceMemory *mem;

		if ((raw & 1) != 0)
			continue;
		mem = dev->getDeviceMemoryWithRegister((UInt8)reg);
		if (mem != NULL && mem->getLength() != 0) {
			info.bars[n].offset = reg;
			info.bars[n].cpu_address = mem->getPhysicalAddress();
			info.bars[n].size = mem->getLength();
			n++;
		} else if (sizeBar(dev, reg, raw, &info.bars[n])) {
			// the family has no range for it, a 64-bit BAR above 4 GB as the firmware placed it
			n++;
		}
		if ((raw & 6) == 4)
			reg += 4;
	}
	info.nbars = n;
	if (n == 0) {
		IOLog("PDNVGPU: %02x:%02x.%x has no memory BAR\n", info.bus, info.slot, info.func);
		return false;
	}

	dev->setMemoryEnable(true);
	dev->setBusMasterEnable(true);

	fGPU = pdnv_gpu_probe(&info);
	if (fGPU == NULL)
		return false;

	// the line interrupt, index 0, as the Linux layer falls back to without MSI
	fWorkLoop = IOWorkLoop::workLoop();
	fInterrupt = IOFilterInterruptEventSource::filterInterruptEventSource(this, action, filter, provider, 0);
	if (fWorkLoop == NULL || fInterrupt == NULL || fWorkLoop->addEventSource(fInterrupt) != kIOReturnSuccess) {
		IOLog("PDNVGPU: no interrupt\n");
		return false;
	}
	fInterrupt->enable();

	if (pdnv_gpu_start(fGPU) != 0)
		return false;
	pdnv_dev_add_gpu(pdnv_gpu_state(fGPU));
	registerService();
	return true;
}

// the top half, at interrupt level
bool
PDNVGPU::filter(OSObject *owner, IOFilterInterruptEventSource *src)
{
	PDNVGPU *self = (PDNVGPU *)owner;

	(void)src;
	return pdnv_gpu_isr(self->fGPU) == 2;
}

void
PDNVGPU::action(OSObject *owner, IOInterruptEventSource *src, int count)
{
	PDNVGPU *self = (PDNVGPU *)owner;

	(void)src;
	(void)count;
	pdnv_gpu_bh(self->fGPU);
}

// DMA-able system memory: one physically contiguous buffer, its kernel address and its physical address

extern "C" void *
pdnv_dma_alloc(uint64_t size, int cache, uint64_t mask, uint64_t *pa, void **va)
{
	IOOptionBits opt = kIODirectionInOut | kIOMemoryPhysicallyContiguous |
	    (cache == PDNV_MAP_CACHED ? kIOMapDefaultCache : cache == PDNV_MAP_WC ? kIOMapWriteCombineCache :
	    kIOMapInhibitCache);
	IOBufferMemoryDescriptor *bmd;

	bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, opt, round_page(size),
	    mask & ~(uint64_t)PAGE_MASK);
	if (bmd == NULL)
		return NULL;
	if (bmd->prepare() != kIOReturnSuccess) {
		bmd->release();
		return NULL;
	}
	*va = bmd->getBytesNoCopy();
	*pa = bmd->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
	bzero(*va, round_page(size));
	return bmd;
}

extern "C" void
pdnv_dma_free(void *handle)
{
	IOBufferMemoryDescriptor *bmd = (IOBufferMemoryDescriptor *)handle;

	bmd->complete();
	bmd->release();
}

extern "C" ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);

extern "C" uint64_t
pdnv_kvtophys(uint64_t va)
{
	ppnum_t pn = pmap_find_phys(kernel_pmap, va);

	return pn == 0 ? 0 : ((uint64_t)pn << PAGE_SHIFT) | (va & PAGE_MASK);
}

// one virtual range over several physical runs, for kernel mappings that span allocation chunks
extern "C" void *
pdnv_map_runs(const uint64_t *pa, uint32_t n, uint64_t run, int cache, void **cookie)
{
	IOOptionBits opt = kIOMapAnywhere | (cache == PDNV_MAP_CACHED ? kIOMapDefaultCache :
	    cache == PDNV_MAP_WC ? kIOMapWriteCombineCache : kIOMapInhibitCache);
	IOAddressRange *r = IONewZero(IOAddressRange, n);
	IOMemoryDescriptor *md;
	IOMemoryMap *map = NULL;

	if (r == NULL)
		return NULL;
	for (uint32_t i = 0; i < n; i++) {
		r[i].address = pa[i];
		r[i].length = run;
	}
	md = IOMemoryDescriptor::withAddressRanges(r, n, kIODirectionInOut | kIOMemoryTypePhysical64, NULL);
	IODelete(r, IOAddressRange, n);
	if (md != NULL) {
		map = md->createMappingInTask(kernel_task, 0, opt);
		md->release();
	}
	if (map == NULL)
		return NULL;
	*cookie = map;
	return (void *)map->getVirtualAddress();
}

extern "C" void
pdnv_unmap_runs(void *cookie)
{
	if (cookie != NULL)
		((IOMemoryMap *)cookie)->release();
}
