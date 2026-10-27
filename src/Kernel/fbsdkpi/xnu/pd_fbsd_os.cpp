// SPDX-License-Identifier: MIT
// the xnu side of the fbsdkpi seam

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOMapper.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pci/IOPCIDevice.h>
#if __has_include(<IOKit/acpi/IOACPIPlatformExpert.h>)
#include <IOKit/acpi/IOACPIPlatformExpert.h>
#define PDOS_HAVE_ACPI 1
#endif
#include <kern/locks.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <libkern/OSAtomic.h>
#include <libkern/OSKextLib.h>
#include <pexpert/pexpert.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/uio.h>

#include "pd_fbsd_os.h"
#include "pd_fbsd_xnu.h"

extern "C" {
extern ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);
extern boolean_t ml_set_interrupts_enabled(boolean_t enable);
extern boolean_t ml_get_interrupts_enabled(void);
}

static lck_grp_t *sGroup;
static lck_spin_t *sWaitLock;
static lck_mtx_t *sListLock;

void
pdos_xnu_init(void)
{
	if (sGroup != NULL)
		return;
	sGroup = lck_grp_alloc_init("fbsdkpi", LCK_GRP_ATTR_NULL);
	sWaitLock = lck_spin_alloc_init(sGroup, LCK_ATTR_NULL);
	sListLock = lck_mtx_alloc_init(sGroup, LCK_ATTR_NULL);
}

// console

void
pdos_log(const char *msg)
{
	IOLog("%s", msg);
}

int
pdos_bootarg_int(const char *name, int def)
{
	int value = def;
	if (!PE_parse_boot_argn(name, &value, sizeof(value)))
		return def;
	return value;
}

// a string boot-arg copied into buf, 0 when it is absent
int
pdos_bootarg_str(const char *name, char *buf, int len)
{
	if (len <= 0)
		return 0;
	buf[0] = 0;
	// PE_parse_boot_argn copies a non-numeric value as a string
	return PE_parse_boot_argn(name, buf, len) ? 1 : 0;
}

int
pdos_debug_enabled(void)
{
	static int sDebug = -1;
	if (sDebug < 0) {
		int value = 0;
		sDebug = PE_parse_boot_argn("fbsdkpi_debug", &value, sizeof(value)) && value != 0;
	}
	return sDebug;
}

void
pdos_panic(const char *msg)
{
	panic("fbsdkpi: %s", msg);
}

// time

int64_t
pdos_uptime_us(void)
{
	uint64_t abs, ns;
	clock_get_uptime(&abs);
	absolutetime_to_nanoseconds(abs, &ns);
	return (int64_t)(ns / 1000);
}

void
pdos_delay_us(uint64_t us)
{
	IODelay((unsigned)us);
}

void
pdos_sleep_us(uint64_t us)
{
	if (us == 0)
		return;
	// inside a kernel fpu section interrupts are off, blocking there would never be woken, so spin
	if (!ml_get_interrupts_enabled()) {
		for (; us > 1000; us -= 1000)
			IODelay(1000);
		IODelay((unsigned)us);
		return;
	}
	static int sSleepEvent;
	assert_wait_timeout((event_t)&sSleepEvent, THREAD_UNINT, (uint32_t)us,
		NSEC_PER_USEC);
	thread_block(THREAD_CONTINUE_NULL);
}

// memory

struct pd_block {
	size_t		size;
	size_t		pad;
};

void *
pdos_malloc(size_t size, int zero, int canwait)
{
	pd_block *block = (pd_block *)IOMalloc(size + sizeof(pd_block));
	if (block == NULL)
		return NULL;
	block->size = size;
	if (zero)
		bzero(block + 1, size);
	return block + 1;
}

void
pdos_free(void *ptr)
{
	if (ptr == NULL)
		return;
	pd_block *block = (pd_block *)ptr - 1;
	IOFree(block, block->size + sizeof(pd_block));
}

size_t
pdos_malloc_size(void *ptr)
{
	return ((pd_block *)ptr - 1)->size;
}

// contiguous blocks and device mappings keep their iokit objects in one list

struct pd_mapping {
	pd_mapping					*next;
	void						*va;
	IOBufferMemoryDescriptor	*buffer;
	IOMemoryDescriptor			*desc;
	IOMemoryMap					*map;
};

static pd_mapping *sMappings;

static void
mapping_add(pd_mapping *m)
{
	lck_mtx_lock(sListLock);
	m->next = sMappings;
	sMappings = m;
	lck_mtx_unlock(sListLock);
}

static pd_mapping *
mapping_take(void *va)
{
	lck_mtx_lock(sListLock);
	pd_mapping **link = &sMappings;
	pd_mapping *found = NULL;
	while (*link != NULL) {
		if ((*link)->va == va) {
			found = *link;
			*link = found->next;
			break;
		}
		link = &(*link)->next;
	}
	lck_mtx_unlock(sListLock);
	return found;
}

void *
pdos_malloc_contig(size_t size, size_t align, uint64_t maxaddr, uint64_t *phys)
{
	if (align < PAGE_SIZE)
		align = PAGE_SIZE;
	mach_vm_address_t mask = maxaddr != 0 ? (mach_vm_address_t)maxaddr
		: 0x0000ffffffffffffULL;
	mask &= ~(mach_vm_address_t)(align - 1);
	IOBufferMemoryDescriptor *buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
		kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
		size, mask);
	if (buffer == NULL)
		return NULL;
	if (buffer->prepare() != kIOReturnSuccess) {
		buffer->release();
		return NULL;
	}
	pd_mapping *m = (pd_mapping *)IOMalloc(sizeof(pd_mapping));
	if (m == NULL) {
		buffer->complete();
		buffer->release();
		return NULL;
	}
	bzero(m, sizeof(*m));
	m->va = buffer->getBytesNoCopy();
	m->buffer = buffer;
	if (phys != NULL)
		*phys = buffer->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
	mapping_add(m);
	return m->va;
}

void
pdos_free_contig(void *ptr, size_t size)
{
	pd_mapping *m = mapping_take(ptr);
	if (m == NULL)
		return;
	if (m->map != NULL) {
		m->map->release();
		m->desc->release();
	}
	m->buffer->complete();
	m->buffer->release();
	IOFree(m, sizeof(pd_mapping));
}

// contiguous memory the cpu sees write-combined, for a device that does not snoop the cpu caches
void *
pdos_malloc_contig_wc(size_t size, size_t align, uint64_t maxaddr, uint64_t *phys)
{
	uint64_t pa = 0;
	void *cached = pdos_malloc_contig(size, align, maxaddr, &pa);
	if (cached == NULL)
		return NULL;
	pd_mapping *m = mapping_take(cached);
	bzero(cached, size);
	// the zeroes reach dram before the write-combined alias is used
	m->buffer->performOperation(kIOMemoryIncoherentIOFlush, 0, size);
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		(IOPhysicalAddress)pa, size, kIODirectionNone | kIOMemoryMapperNone);
	IOMemoryMap *map = desc != NULL ? desc->map(kIOMapAnywhere | kIOMapWriteCombineCache) : NULL;
	if (map == NULL) {
		if (desc != NULL)
			desc->release();
		m->buffer->complete();
		m->buffer->release();
		IOFree(m, sizeof(pd_mapping));
		return NULL;
	}
	m->desc = desc;
	m->map = map;
	m->va = (void *)map->getVirtualAddress();
	mapping_add(m);
	if (phys != NULL)
		*phys = pa;
	return m->va;
}

extern "C" void flush_dcache64(addr64_t addr, unsigned count, int phys);

// clean and invalidate the cpu caches over a physical range through the physmap, no allocation
extern "C" void
pdos_dma_sync_phys(uint64_t phys, size_t len)
{
	if (len != 0)
		flush_dcache64((addr64_t)phys, (unsigned)len, TRUE);
}

uint64_t
pdos_vtophys(const void *va)
{
	uint64_t page = (uint64_t)pmap_find_phys(kernel_pmap, (addr64_t)(uintptr_t)va);
	return (page << PAGE_SHIFT) | ((uintptr_t)va & PAGE_MASK);
}

// BARs of the attached devices, each mapped whole once through IOPCIFamily, so
// an ioremap of a range inside one (a 64-bit vram aperture) gets a working view
#define PD_BAR_DEVS	4
struct pd_bar_window {
	uint64_t phys;
	uint64_t size;
	IOMemoryMap *map;
};
static IOPCIDevice *sBarDevs[PD_BAR_DEVS];
static pd_bar_window sBarWindows[PD_BAR_DEVS][6];

void
pdos_pci_note(void *dev)
{
	for (int i = 0; i < PD_BAR_DEVS; i++) {
		if (sBarDevs[i] == dev)
			return;
		if (sBarDevs[i] == NULL) {
			sBarDevs[i] = (IOPCIDevice *)dev;
			return;
		}
	}
}

static void *mapping_track(IOMemoryDescriptor *desc, IOMemoryMap *map, uint64_t offset);

// a bar this large is mapped a piece at a time, a whole 16 GB vram bar in 4 KB pages would stall the vm for minutes
#define PD_BAR_WHOLE_MAX	(256ULL << 20)
#define PD_MAP_MAX	(1ULL << 30)

static void *
bar_window_va(uint64_t phys, size_t size, IOOptionBits cacheMode)
{
	for (int d = 0; d < PD_BAR_DEVS && sBarDevs[d] != NULL; d++) {
		for (int bar = 0; bar < 6; bar++) {
			pd_bar_window *w = &sBarWindows[d][bar];
			if (w->size == 0) {
				IODeviceMemory *mem = sBarDevs[d]->getDeviceMemoryWithRegister(
					kIOPCIConfigBaseAddress0 + bar * 4);
				if (mem == NULL)
					continue;
				w->phys = mem->getPhysicalAddress();
				w->size = mem->getLength();
			}
			if (phys < w->phys || phys + size > w->phys + w->size)
				continue;
			// boot-arg pd_bar_whole=1 maps every bar whole as before, to compare against the piece maps
			int whole = 0;
			PE_parse_boot_argn("pd_bar_whole", &whole, sizeof(whole));
			if (w->size > PD_BAR_WHOLE_MAX && !whole) {
				IODeviceMemory *mem = sBarDevs[d]->getDeviceMemoryWithRegister(
					kIOPCIConfigBaseAddress0 + bar * 4);
				uint64_t start = trunc_page_64(phys - w->phys);
				uint64_t len = round_page_64(phys - w->phys + size) - start;
				IOMemoryDescriptor *sub;
				IOMemoryMap *map;

				if (mem == NULL)
					return NULL;
				sub = IOSubMemoryDescriptor::withSubRange(mem, start, len, kIODirectionNone);
				if (sub == NULL)
					return NULL;
				if (len >= PD_BAR_WHOLE_MAX)
					IOLog("pdos: mapping bar %d piece 0x%llx+0x%llx\n", bar, start, len);
				map = sub->map(kIOMapAnywhere | cacheMode);
				if (len >= PD_BAR_WHOLE_MAX)
					IOLog("pdos: bar %d piece mapped at %p\n", bar, map ? (void *)map->getVirtualAddress() : NULL);
				if (map == NULL) {
					sub->release();
					return NULL;
				}
				return mapping_track(sub, map, (phys - w->phys) - start);
			}
			if (w->map == NULL) {
				w->map = sBarDevs[d]->mapDeviceMemoryWithRegister(
					kIOPCIConfigBaseAddress0 + bar * 4, kIOMapInhibitCache);
				if (w->map == NULL)
					return NULL;
			}
			return (void *)(w->map->getVirtualAddress() + (phys - w->phys));
		}
	}
	return NULL;
}

// records a mapping so pdos_unmap_phys finds it by the address it returns
static void *
mapping_track(IOMemoryDescriptor *desc, IOMemoryMap *map, uint64_t offset)
{
	pd_mapping *m = (pd_mapping *)IOMalloc(sizeof(pd_mapping));

	if (m == NULL) {
		map->release();
		desc->release();
		return NULL;
	}
	bzero(m, sizeof(*m));
	m->va = (void *)(map->getVirtualAddress() + offset);
	m->desc = desc;
	m->map = map;
	mapping_add(m);
	return m->va;
}

void *
pdos_map_phys(uint64_t phys, size_t size, int uncached)
{
	// one 16 GB uncached map hangs real hardware in map(), callers fall back to smaller maps
	if (size > PD_MAP_MAX) {
		IOLog("pdos: not mapping 0x%llx+0x%lx in one piece, over the 1 GB limit\n", phys, size);
		return NULL;
	}
	// 2 asks for write-combining, the way ioremap_wc maps a vram aperture
	IOOptionBits cacheMode = uncached == 2 ? kIOMapWriteCombineCache : uncached ? kIOMapInhibitCache : 0;
	void *bar_va = bar_window_va(phys, size, cacheMode == 0 ? kIOMapInhibitCache : cacheMode);
	if (bar_va != NULL)
		return bar_va;

	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		(IOPhysicalAddress)phys, size, kIODirectionNone | kIOMemoryMapperNone);
	if (desc == NULL)
		return NULL;
	IOMemoryMap *map = desc->map(kIOMapAnywhere | cacheMode);
	if (map == NULL) {
		desc->release();
		return NULL;
	}
	pd_mapping *m = (pd_mapping *)IOMalloc(sizeof(pd_mapping));
	if (m == NULL) {
		map->release();
		desc->release();
		return NULL;
	}
	bzero(m, sizeof(*m));
	m->va = (void *)map->getVirtualAddress();
	m->desc = desc;
	m->map = map;
	mapping_add(m);
	return m->va;
}

void
pdos_unmap_phys(void *va, size_t size)
{
	pd_mapping *m = mapping_take(va);
	if (m == NULL)
		return;
	m->map->release();
	if (m->desc != NULL)
		m->desc->release();
	IOFree(m, sizeof(pd_mapping));
}

// scattered pages joined into one kernel range, released through pdos_unmap_phys
void *
pdos_map_pages(const uint64_t *phys, size_t count, int uncached)
{
	IOAddressRange *ranges = (IOAddressRange *)IOMalloc(count * sizeof(IOAddressRange));
	if (ranges == NULL)
		return NULL;
	for (size_t i = 0; i < count; i++) {
		ranges[i].address = phys[i];
		ranges[i].length = PAGE_SIZE;
	}
	IOMemoryDescriptor *desc = IOMemoryDescriptor::withAddressRanges(ranges, (UInt32)count,
		kIODirectionInOut | kIOMemoryTypePhysical64 | kIOMemoryMapperNone, NULL);
	IOFree(ranges, count * sizeof(IOAddressRange));
	if (desc == NULL)
		return NULL;
	IOMemoryMap *map = desc->createMappingInTask(kernel_task, 0,
		kIOMapAnywhere | (uncached ? kIOMapWriteCombineCache : 0));
	if (map == NULL) {
		desc->release();
		return NULL;
	}
	pd_mapping *m = (pd_mapping *)IOMalloc(sizeof(pd_mapping));
	if (m == NULL) {
		map->release();
		desc->release();
		return NULL;
	}
	bzero(m, sizeof(*m));
	m->va = (void *)map->getVirtualAddress();
	m->desc = desc;
	m->map = map;
	mapping_add(m);
	return m->va;
}

// releases a pdos_map_pages range, any other address is left alone
void
pdos_unmap_pages(void *va)
{
	lck_mtx_lock(sListLock);
	pd_mapping **link = &sMappings;
	pd_mapping *found = NULL;
	while (*link != NULL) {
		if ((*link)->va == va && (*link)->buffer == NULL && (*link)->desc != NULL) {
			found = *link;
			*link = found->next;
			break;
		}
		link = &(*link)->next;
	}
	lck_mtx_unlock(sListLock);
	if (found == NULL)
		return;
	found->map->release();
	found->desc->release();
	IOFree(found, sizeof(pd_mapping));
}

// copies to and from the calling task's memory, 0 or an errno
int
pdos_copyin(const void *uaddr, void *kaddr, size_t len)
{
	return copyin((user_addr_t)(uintptr_t)uaddr, kaddr, len);
}

int
pdos_copyout(const void *kaddr, void *uaddr, size_t len)
{
	return copyout(kaddr, (user_addr_t)(uintptr_t)uaddr, len);
}

// a BAR through IOPCIFamily, which knows where it decodes even above the
// physical range a plain physical-address descriptor can map
uint64_t
pdos_pci_bar_map(void *dev, int bar, uint64_t *size, uint64_t *phys)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(
		kIOPCIConfigBaseAddress0 + bar * 4, kIOMapInhibitCache);
	if (map == NULL)
		return 0;

	pd_mapping *m = (pd_mapping *)IOMalloc(sizeof(pd_mapping));
	if (m == NULL) {
		map->release();
		return 0;
	}
	bzero(m, sizeof(*m));
	m->va = (void *)map->getVirtualAddress();
	m->map = map;
	mapping_add(m);
	if (size != NULL)
		*size = map->getLength();
	if (phys != NULL)
		*phys = map->getPhysicalAddress();
	return (uint64_t)m->va;
}

void
pdos_pci_bar_unmap(void *dev, int bar)
{
	// mappings are released by address through pdos_unmap_phys
}

// firmware files kept in the kext bundle instead of the kernel collection

#define PDOS_STR2(x)	#x
#define PDOS_STR(x)		PDOS_STR2(x)
#define PDOS_FIRMWARE_WAIT_S	90

extern "C" vnode_t vfs_rootvnode(void);

static bool
root_mounted(void)
{
	vnode_t root = vfs_rootvnode();
	if (root == NULLVP)
		return false;
	vnode_put(root);
	return true;
}

int
pdos_load_firmware(const char *name, void **data, size_t *size)
{
	char path[MAXPATHLEN];

	// a driver attaching before the root mount waits for it
	for (int i = 0; i < PDOS_FIRMWARE_WAIT_S * 10 && !root_mounted(); i++)
		IOSleep(100);
	if (!root_mounted())
		return ENOENT;

	// the shared firmware packages first, then the kext's own bundle
	vfs_context_t ctx = vfs_context_create(NULL);
	vnode_t vp = NULLVP;
	snprintf(path, sizeof(path), "/Library/Firmware/%s", name);
	int error = vnode_open(path, FREAD, 0, 0, &vp, ctx);
	if (error != 0) {
		snprintf(path, sizeof(path), "/System/Library/Extensions/%s.kext/Contents/Resources/firmware/%s",
			PDOS_STR(PDFBSD_CLASS), name);
		error = vnode_open(path, FREAD, 0, 0, &vp, ctx);
	}
	if (error != 0) {
		vfs_context_rele(ctx);
		return error;
	}
	if (pdos_debug_enabled())
		IOLog("fbsdkpi: firmware %s from %s\n", name, path);

	struct vnode_attr va;
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	error = vnode_getattr(vp, &va, ctx);
	if (error == 0 && (va.va_data_size == 0 || va.va_data_size > 64 * 1024 * 1024))
		error = EFBIG;

	void *buffer = NULL;
	size_t length = error == 0 ? (size_t)va.va_data_size : 0;
	if (error == 0) {
		buffer = IOMallocData(length);
		if (buffer == NULL)
			error = ENOMEM;
	}
	if (error == 0) {
		int resid = 0;
		error = vn_rdwr(UIO_READ, vp, (caddr_t)buffer, (int)length, 0, UIO_SYSSPACE,
			0, vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
		if (error == 0 && resid != 0)
			error = EIO;
	}

	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (error != 0) {
		if (buffer != NULL)
			IOFreeData(buffer, length);
		return error;
	}
	*data = buffer;
	*size = length;
	return 0;
}

void
pdos_free_firmware(void *data, size_t size)
{
	if (data != NULL)
		IOFreeData(data, size);
}

// dma through the device's iommu mapper

struct pd_dma_cookie {
	IODMACommand		*cmd;
	IOMemoryDescriptor	*md;
	// set when the range was copied below the device's address limit
	void				*bounce;
	uint64_t			bounce_len;
};

// without a mapper the bus address is the physical one and no state is kept
static pd_dma_cookie sIdentityCookie;

void *
pdos_dma_map_phys(void *pcidev, uint64_t phys, uint64_t len, int addrbits,
	uint64_t *busaddr)
{
	IOService *device = (IOService *)pcidev;
	IOMapper *mapper = device != NULL ? IOMapper::copyMapperForDevice(device) : NULL;
	bool reachable = addrbits >= 64 || phys + len <= (1ULL << addrbits);
	if (mapper == NULL && reachable) {
		*busaddr = phys;
		return &sIdentityCookie;
	}

	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(
		(IOPhysicalAddress)phys, len, kIODirectionInOut);
	if (md == NULL) {
		if (mapper != NULL)
			mapper->release();
		return NULL;
	}

	// without a mapper the range is bounced below the limit, copied through
	// the physical descriptor so the original needs no mapping
	if (mapper == NULL) {
		pd_dma_cookie *cookie = (pd_dma_cookie *)IOMallocZero(sizeof(pd_dma_cookie));
		uint64_t bphys = 0;
		void *bounce = cookie != NULL ? pdos_malloc_contig(len, PAGE_SIZE,
			(1ULL << addrbits) - 1, &bphys) : NULL;
		if (bounce == NULL) {
			if (cookie != NULL)
				IOFree(cookie, sizeof(pd_dma_cookie));
			md->release();
			return NULL;
		}
		md->readBytes(0, bounce, len);
		cookie->md = md;
		cookie->bounce = bounce;
		cookie->bounce_len = len;
		*busaddr = bphys;
		return cookie;
	}

	// the command keeps its own reference on the mapper
	IODMACommand *cmd = IODMACommand::withSpecification(kIODMACommandOutputHost64,
		addrbits >= 64 ? 64 : addrbits, 0,
		IODMACommand::kMapped, 0, 1, mapper, NULL);
	mapper->release();
	if (cmd == NULL) {
		md->release();
		return NULL;
	}
	if (cmd->setMemoryDescriptor(md, true) != kIOReturnSuccess) {
		cmd->release();
		md->release();
		return NULL;
	}

	// a mapped range comes back as one iova segment
	UInt64 offset = 0;
	IODMACommand::Segment64 segment;
	UInt32 count = 1;
	if (cmd->gen64IOVMSegments(&offset, &segment, &count) != kIOReturnSuccess
		|| count != 1 || segment.fLength < len) {
		cmd->clearMemoryDescriptor(true);
		cmd->release();
		md->release();
		return NULL;
	}

	pd_dma_cookie *cookie = (pd_dma_cookie *)IOMallocZero(sizeof(pd_dma_cookie));
	if (cookie == NULL) {
		cmd->clearMemoryDescriptor(true);
		cmd->release();
		md->release();
		return NULL;
	}
	cookie->cmd = cmd;
	cookie->md = md;
	*busaddr = segment.fIOVMAddr;
	return cookie;
}

void
pdos_dma_sync(void *opaque, int to_device)
{
	pd_dma_cookie *cookie = (pd_dma_cookie *)opaque;
	if (cookie == NULL || cookie == &sIdentityCookie)
		return;

	if (cookie->bounce != NULL) {
		if (to_device)
			cookie->md->readBytes(0, cookie->bounce, cookie->bounce_len);
		else
			cookie->md->writeBytes(0, cookie->bounce, cookie->bounce_len);
		return;
	}
	cookie->cmd->synchronize(to_device ? kIODirectionOut : kIODirectionIn);
}

int
pdos_dma_has_mapper(void *pcidev)
{
	IOMapper *mapper = pcidev != NULL ? IOMapper::copyMapperForDevice((IOService *)pcidev) : NULL;
	if (mapper == NULL)
		return 0;
	mapper->release();
	return 1;
}

void
pdos_dma_unmap(void *opaque)
{
	pd_dma_cookie *cookie = (pd_dma_cookie *)opaque;
	if (cookie == NULL || cookie == &sIdentityCookie)
		return;

	if (cookie->bounce != NULL) {
		pdos_free_contig(cookie->bounce, cookie->bounce_len);
		cookie->md->release();
		IOFree(cookie, sizeof(pd_dma_cookie));
		return;
	}
	cookie->cmd->clearMemoryDescriptor(true);
	cookie->cmd->release();
	cookie->md->release();
	IOFree(cookie, sizeof(pd_dma_cookie));
}

// locks

void *
pdos_mtx_alloc(const char *name)
{
	pdos_xnu_init();
	return lck_mtx_alloc_init(sGroup, LCK_ATTR_NULL);
}

void
pdos_mtx_free(void *m)
{
	lck_mtx_free((lck_mtx_t *)m, sGroup);
}

void
pdos_mtx_lock(void *m)
{
	lck_mtx_lock((lck_mtx_t *)m);
}

int
pdos_mtx_trylock(void *m)
{
	return lck_mtx_try_lock((lck_mtx_t *)m);
}

void
pdos_mtx_unlock(void *m)
{
	lck_mtx_unlock((lck_mtx_t *)m);
}

void *
pdos_spin_alloc(void)
{
	pdos_xnu_init();
	return lck_spin_alloc_init(sGroup, LCK_ATTR_NULL);
}

void
pdos_spin_free(void *s)
{
	lck_spin_free((lck_spin_t *)s, sGroup);
}

void
pdos_spin_lock(void *s)
{
	lck_spin_lock((lck_spin_t *)s);
}

void
pdos_spin_unlock(void *s)
{
	lck_spin_unlock((lck_spin_t *)s);
}

int
pdos_intr_disable(void)
{
	return ml_set_interrupts_enabled(FALSE);
}

void
pdos_intr_restore(int state)
{
	ml_set_interrupts_enabled(state ? TRUE : FALSE);
}

void *
pdos_rw_alloc(void)
{
	pdos_xnu_init();
	return lck_rw_alloc_init(sGroup, LCK_ATTR_NULL);
}

void
pdos_rw_free(void *rw)
{
	lck_rw_free((lck_rw_t *)rw, sGroup);
}

void
pdos_rw_rlock(void *rw)
{
	lck_rw_lock_shared((lck_rw_t *)rw);
}

void
pdos_rw_runlock(void *rw)
{
	lck_rw_unlock_shared((lck_rw_t *)rw);
}

void
pdos_rw_wlock(void *rw)
{
	lck_rw_lock_exclusive((lck_rw_t *)rw);
}

void
pdos_rw_wunlock(void *rw)
{
	lck_rw_unlock_exclusive((lck_rw_t *)rw);
}

int
pdos_rw_try_rlock(void *rw)
{
	return lck_rw_try_lock((lck_rw_t *)rw, LCK_RW_TYPE_SHARED);
}

int
pdos_rw_try_wlock(void *rw)
{
	return lck_rw_try_lock((lck_rw_t *)rw, LCK_RW_TYPE_EXCLUSIVE);
}

void
pdos_rw_downgrade(void *rw)
{
	lck_rw_lock_exclusive_to_shared((lck_rw_t *)rw);
}

// the global wait lock, interrupts stay off while it is held so interrupt
// handlers can wake waiters through it

int
pdos_wait_lock(void)
{
	pdos_xnu_init();
	int state = ml_set_interrupts_enabled(FALSE);
	lck_spin_lock(sWaitLock);
	return state;
}

void
pdos_wait_unlock(int state)
{
	lck_spin_unlock(sWaitLock);
	ml_set_interrupts_enabled(state ? TRUE : FALSE);
}

int
pdos_wait_sleep(const void *event, int64_t timeout_us, int interruptible, int *state)
{
	wait_interrupt_t level = interruptible ? THREAD_ABORTSAFE : THREAD_UNINT;
	// long timeouts are clamped, callers recheck their deadline after waking
	if (timeout_us > 0xf0000000LL)
		timeout_us = 0xf0000000LL;
	if (timeout_us > 0)
		assert_wait_timeout((event_t)event, level, (uint32_t)timeout_us, NSEC_PER_USEC);
	else
		assert_wait((event_t)event, level);
	lck_spin_unlock(sWaitLock);
	ml_set_interrupts_enabled(*state ? TRUE : FALSE);

	wait_result_t result = thread_block(THREAD_CONTINUE_NULL);

	*state = ml_set_interrupts_enabled(FALSE);
	lck_spin_lock(sWaitLock);
	if (result == THREAD_TIMED_OUT)
		return PDOS_WAIT_TIMEOUT;
	if (result == THREAD_INTERRUPTED)
		return PDOS_WAIT_INTR;
	return PDOS_WAIT_OK;
}

void
pdos_wakeup(const void *event, int all)
{
	thread_wakeup_prim((event_t)event, all ? FALSE : TRUE, THREAD_AWAKENED);
}

// threads

struct pd_start {
	void	(*fn)(void *);
	void	*arg;
};

static void
thread_entry(void *param, wait_result_t result)
{
	pd_start start = *(pd_start *)param;
	IOFree(param, sizeof(pd_start));
	start.fn(start.arg);
	pdos_thread_exit();
}

void *
pdos_thread_start(void (*fn)(void *), void *arg)
{
	pd_start *start = (pd_start *)IOMalloc(sizeof(pd_start));
	if (start == NULL)
		return NULL;
	start->fn = fn;
	start->arg = arg;
	thread_t thread;
	if (kernel_thread_start(thread_entry, start, &thread) != KERN_SUCCESS) {
		IOFree(start, sizeof(pd_start));
		return NULL;
	}
	thread_deallocate(thread);
	return thread;
}

void *
pdos_thread_current(void)
{
	return current_thread();
}

void
pdos_thread_exit(void)
{
	static int sExitEvent;
	thread_terminate(current_thread());
	for (;;) {
		assert_wait((event_t)&sExitEvent, THREAD_UNINT);
		thread_block(THREAD_CONTINUE_NULL);
	}
}

void
pdos_thread_set_priority(void *thread, int prio)
{
}

int
pdos_cpu_count(void)
{
	return 1;
}

int
pdos_cpu_number(void)
{
	return 0;
}

// kernel fpu: dc's display mode library runs sse code between begin and end, with interrupts off so
// nothing else touches the registers. depth, saved state and the interrupt flag belong to the thread, a
// thread that blocks inside a section resumes elsewhere and still ends its own section
#if defined(__x86_64__)
#define PD_FPU_SLOTS 64

struct pd_fpu_save {
	uint8_t area[512] __attribute__((aligned(16)));
	thread_t thread;
	int depth;
};
static pd_fpu_save pd_fpu_saves[PD_FPU_SLOTS];

extern "C" void _disable_preemption(void);
extern "C" void _enable_preemption(void);

// preemption is off here, so the scan and the claim cannot race on this cpu
static pd_fpu_save *
pd_fpu_slot(thread_t thread, bool claim)
{
	pd_fpu_save *free_slot = NULL;

	for (int i = 0; i < PD_FPU_SLOTS; i++) {
		if (pd_fpu_saves[i].thread == thread)
			return &pd_fpu_saves[i];
		if (free_slot == NULL && pd_fpu_saves[i].thread == NULL)
			free_slot = &pd_fpu_saves[i];
	}
	if (claim && free_slot != NULL &&
	    OSCompareAndSwapPtr(NULL, thread, (void * volatile *)&free_slot->thread))
		return free_slot;
	// another cpu took the free slot first, look again, a full table returns nothing
	return claim && free_slot != NULL ? pd_fpu_slot(thread, true) : NULL;
}

// like linux this only holds off preemption, interrupts stay on for the whole section
extern "C" void
lkpi_kernel_fpu_begin(void)
{
	pd_fpu_save *s;
	uint32_t mxcsr = 0x1f80;

	_disable_preemption();
	s = pd_fpu_slot(current_thread(), true);
	if (s == NULL) {
		_enable_preemption();
		return;
	}
	if (s->depth++ > 0) {
		_enable_preemption();
		return;
	}
	__asm__ volatile("fxsave64 %0" : "=m"(s->area));
	__asm__ volatile("fninit; ldmxcsr %0" : : "m"(mxcsr));
}

extern "C" void
lkpi_kernel_fpu_end(void)
{
	pd_fpu_save *s = pd_fpu_slot(current_thread(), false);

	if (s == NULL)
		return;
	if (--s->depth > 0)
		return;
	__asm__ volatile("fxrstor64 %0" : : "m"(s->area));
	s->thread = NULL;
	_enable_preemption();
}
#endif

// points the platform console at a mapped framebuffer, so a debug boot can watch text land on the gpu's own scanout
extern "C" void
pdos_console_redirect(void *va, uint32_t width, uint32_t height, uint32_t pitch)
{
	IOPlatformExpert *pe = OSDynamicCast(IOPlatformExpert, IOService::getPlatform());
	PE_Video video;

	if (pe == NULL || va == NULL)
		return;
	bzero(&video, sizeof(video));
	if (pe->getConsoleInfo(&video) != kIOReturnSuccess)
		bzero(&video, sizeof(video));
	video.v_baseAddr = (unsigned long)va;
	video.v_rowBytes = pitch;
	video.v_width = width;
	video.v_height = height;
	video.v_depth = 32;
	pe->setConsoleInfo(&video, kPEEnableScreen);
}

#ifdef PDOS_HAVE_ACPI
// getACPITableData is protected, a derived class may name it
struct pd_acpi_tables : IOACPIPlatformExpert {
	static const OSData *
	get(IOACPIPlatformExpert *pe, const char *signature, uint32_t instance)
	{
		return (pe->*(&pd_acpi_tables::getACPITableData))(signature, instance);
	}
};

// acpi tables come from the platform expert, as a copy the caller keeps
int
pdos_acpi_get_table(const char *signature, uint32_t instance, void **data,
	size_t *size)
{
	IOService *platform = IOService::getPlatform();
	if (platform == NULL || platform->metaCast("IOACPIPlatformExpert") == NULL)
		return ENOENT;

	const OSData *table = pd_acpi_tables::get(
		static_cast<IOACPIPlatformExpert *>(platform), signature, instance);
	if (table == NULL)
		return ENOENT;

	*size = table->getLength();
	*data = IOMallocData(*size);
	if (*data != NULL)
		memcpy(*data, table->getBytesNoCopy(), *size);
	table->release();
	return *data != NULL ? 0 : ENOMEM;
}

void
pdos_acpi_put_table(void *data, size_t size)
{
	if (data != NULL)
		IOFreeData(data, size);
}
#else
// drivers built without the acpi family headers see no tables
int
pdos_acpi_get_table(const char *signature, uint32_t instance, void **data,
	size_t *size)
{
	return ENOENT;
}

void
pdos_acpi_put_table(void *data, size_t size)
{
}
#endif

// timer callbacks

void *
pdos_tcall_alloc(void (*fn)(void *, void *), void *arg0)
{
	return thread_call_allocate((thread_call_func_t)fn, arg0);
}

void
pdos_tcall_free(void *tc)
{
	thread_call_cancel((thread_call_t)tc);
	thread_call_free((thread_call_t)tc);
}

void
pdos_tcall_enter_us(void *tc, void *arg1, uint64_t us)
{
	uint64_t deadline;
	clock_interval_to_deadline((uint32_t)us, NSEC_PER_USEC, &deadline);
	thread_call_enter1_delayed((thread_call_t)tc, arg1, deadline);
}

int
pdos_tcall_cancel(void *tc)
{
	return thread_call_cancel((thread_call_t)tc);
}

// pci

uint32_t
pdos_pci_read(void *dev, uint32_t off, int size)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	switch (size) {
		case 1: return pci->configRead8(off);
		case 2: return pci->configRead16(off);
		default: return pci->configRead32(off);
	}
}

void
pdos_pci_write(void *dev, uint32_t off, int size, uint32_t val)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	switch (size) {
		case 1: pci->configWrite8(off, val); break;
		case 2: pci->configWrite16(off, val); break;
		default: pci->configWrite32(off, val); break;
	}
}

int
pdos_pci_location(void *dev, uint8_t *bus, uint8_t *slot, uint8_t *func)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	*bus = pci->getBusNumber();
	*slot = pci->getDeviceNumber();
	*func = pci->getFunctionNumber();
	return 0;
}

// every pci device in the registry, in bus order, for drivers that look at
// devices they are not attached to (the host bridge, the pch)
int
pdos_pci_enumerate(void **devs, int max)
{
	OSDictionary *match = IOService::serviceMatching("IOPCIDevice");
	OSIterator *it = match != NULL ? IOService::getMatchingServices(match) : NULL;
	int n = 0;

	OSSafeReleaseNULL(match);
	if (it == NULL)
		return 0;
	OSObject *obj;
	while ((obj = it->getNextObject()) != NULL && n < max) {
		IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, obj);
		if (pci == NULL)
			continue;
		// the registry keeps the device, a peer only reads and writes its config space
		uint32_t key = ((uint32_t)pci->getBusNumber() << 16) |
		    ((uint32_t)pci->getDeviceNumber() << 8) | pci->getFunctionNumber();
		int i = n++;
		while (i > 0) {
			IOPCIDevice *prev = (IOPCIDevice *)devs[i - 1];
			uint32_t pkey = ((uint32_t)prev->getBusNumber() << 16) |
			    ((uint32_t)prev->getDeviceNumber() << 8) | prev->getFunctionNumber();
			if (pkey <= key)
				break;
			devs[i] = devs[i - 1];
			i--;
		}
		devs[i] = pci;
	}
	it->release();
	return n;
}

uint32_t
pdos_pci_io_read(void *dev, uint32_t port, int size)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	switch (size) {
		case 1: return pci->ioRead8(port);
		case 2: return pci->ioRead16(port);
		default: return pci->ioRead32(port);
	}
}

void
pdos_pci_io_write(void *dev, uint32_t port, int size, uint32_t val)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	switch (size) {
		case 1: pci->ioWrite8(port, val); break;
		case 2: pci->ioWrite16(port, val); break;
		default: pci->ioWrite32(port, val); break;
	}
}

// IOPCIFamily lists the legacy line and any message signalled vectors as
// interrupt indexes, it uses msi-x only for devices without an msi capability

static int
interrupt_indexes(IOPCIDevice *pci, int *legacy, int *messaged, int max)
{
	int count = 0;
	*legacy = -1;
	for (int i = 0; i < 64; i++) {
		int type = 0;
		if (pci->getInterruptType(i, &type) != kIOReturnSuccess)
			break;
		if ((type & kIOInterruptTypePCIMessaged) != 0) {
			if (count < max)
				messaged[count] = i;
			count++;
		} else if (*legacy < 0)
			*legacy = i;
	}
	return count;
}

static bool
has_msi_capability(IOPCIDevice *pci)
{
	UInt8 offset = 0;
	return pci->findPCICapability(kIOPCIMSICapability, &offset) != 0;
}

int
pdos_pci_msi_count(void *dev)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	int legacy, messaged[16];
	if (!has_msi_capability(pci))
		return 0;
	return interrupt_indexes(pci, &legacy, messaged, 16);
}

int
pdos_pci_msix_count(void *dev)
{
	IOPCIDevice *pci = (IOPCIDevice *)dev;
	int legacy, messaged[16];
	if (has_msi_capability(pci))
		return 0;
	return interrupt_indexes(pci, &legacy, messaged, 16);
}

// index 0 is the legacy line, n is the nth message signalled vector
static int
iokit_interrupt_index(IOPCIDevice *pci, int index)
{
	int legacy, messaged[16];
	int count = interrupt_indexes(pci, &legacy, messaged, 16);
	if (index == 0)
		return legacy >= 0 ? legacy : 0;
	if (index - 1 < count && index - 1 < 16)
		return messaged[index - 1];
	return -1;
}

// interrupts, the filter only signals and the handler runs on the work loop thread

class PDFBSD_INTR_CLASS : public OSObject
{
	OSDeclareDefaultStructors(PDFBSD_INTR_CLASS);
public:
	pdos_intr_fn					fn;
	void							*arg;
	// platform nubs run the handler on the work loop, linuxkpi locks there may sleep
	bool							threaded;
	IOFilterInterruptEventSource	*source;
	IOWorkLoop						*workLoop;

	bool initIntr() { return OSObject::init(); }

	static bool filter(OSObject *owner, IOFilterInterruptEventSource *source)
	{
		PDFBSD_INTR_CLASS *self = (PDFBSD_INTR_CLASS *)owner;
		if (self->threaded)
			return true;
		self->fn(self->arg);
		return false;
	}

	static void action(OSObject *owner, IOInterruptEventSource *source, int count)
	{
		PDFBSD_INTR_CLASS *self = (PDFBSD_INTR_CLASS *)owner;
		if (self->threaded)
			self->fn(self->arg);
	}
};

OSDefineMetaClassAndStructors(PDFBSD_INTR_CLASS, OSObject);

void *
pdos_intr_install(void *dev, int index, pdos_intr_fn fn, void *arg)
{
	IOService *nub = (IOService *)dev;
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, nub);
	PDFBSD_INTR_CLASS *intr = OSTypeAlloc(PDFBSD_INTR_CLASS);
	if (intr == NULL || !intr->initIntr()) {
		if (intr != NULL)
			intr->release();
		return NULL;
	}
	intr->fn = fn;
	intr->arg = arg;
	// linuxkpi spinlocks are sleeping mutexes as in a freebsd ithread, so handlers run on the work
	// loop thread, a handler in the filter that met a held lock would stop the cpu with interrupts off
	intr->threaded = true;
	// boot-arg pd_intr_filter=1 runs pci handlers in the filter as before, to compare
	int in_filter = 0;
	if (pci != NULL && PE_parse_boot_argn("pd_intr_filter", &in_filter, sizeof(in_filter)) && in_filter)
		intr->threaded = false;
	intr->workLoop = IOWorkLoop::workLoop();
	// a device tree nub numbers its interrupts as the node lists them
	int iokitIndex = pci != NULL ? iokit_interrupt_index(pci, index) : index;
	if (iokitIndex < 0) {
		intr->release();
		return NULL;
	}
	intr->source = IOFilterInterruptEventSource::filterInterruptEventSource(intr,
		&PDFBSD_INTR_CLASS::action, &PDFBSD_INTR_CLASS::filter, nub, iokitIndex);
	if (intr->workLoop == NULL || intr->source == NULL
			|| intr->workLoop->addEventSource(intr->source) != kIOReturnSuccess) {
		IOLog("fbsdkpi: interrupt %d (iokit %d) could not be registered\n", index, iokitIndex);
		OSSafeReleaseNULL(intr->source);
		OSSafeReleaseNULL(intr->workLoop);
		intr->release();
		return NULL;
	}
	IOLog("fbsdkpi: interrupt %d of %s uses iokit source %d%s\n", index, nub->getName(), iokitIndex,
	    pci != NULL && index > 0 ? " (msi)" : pci != NULL ? " (legacy line)" : "");
	// boot-arg pdamdgpu_intr_off=1 leaves an amd gpu's interrupt disabled, to tell a freeze in interrupt
	// handling from one in the driver's own work, the driver's waits then time out instead
	int off = 0;
	if (pci != NULL && pci->configRead16(kIOPCIConfigVendorID) == 0x1002 &&
	    PE_parse_boot_argn("pdamdgpu_intr_off", &off, sizeof(off)) && off) {
		IOLog("fbsdkpi: pdamdgpu_intr_off=1, interrupt %d of %s stays disabled\n", index, nub->getName());
		return intr;
	}
	intr->source->enable();
	return intr;
}

void
pdos_intr_remove(void *cookie)
{
	PDFBSD_INTR_CLASS *intr = (PDFBSD_INTR_CLASS *)cookie;
	if (intr == NULL)
		return;
	intr->source->disable();
	intr->workLoop->removeEventSource(intr->source);
	OSSafeReleaseNULL(intr->source);
	OSSafeReleaseNULL(intr->workLoop);
	intr->release();
}

// device tree access for platform devices

int
pdos_dt_prop(void *node, const char *name, const void **data, uint32_t *len)
{
	IORegistryEntry *e = (IORegistryEntry *)node;
	OSData *d = e != NULL ? OSDynamicCast(OSData, e->getProperty(name)) : NULL;
	if (d == NULL)
		return -1;
	*data = d->getBytesNoCopy();
	*len = d->getLength();
	return 0;
}

void *
pdos_dt_parent(void *node)
{
	IORegistryEntry *e = (IORegistryEntry *)node;
	return e != NULL ? e->getParentEntry(gIODTPlane) : NULL;
}

void *
pdos_dt_child(void *node, int index)
{
	IORegistryEntry *e = (IORegistryEntry *)node, *found = NULL;
	if (e == NULL)
		return NULL;
	// the registry changes while drivers match, an iterator that saw a change ends early and goes invalid
	for (int tries = 0; tries < 16; tries++) {
		OSIterator *it = e->getChildIterator(gIODTPlane);
		if (it == NULL)
			return NULL;
		found = NULL;
		for (int i = 0; OSObject *o = it->getNextObject(); i++) {
			if (i == index) {
				found = OSDynamicCast(IORegistryEntry, o);
				break;
			}
		}
		bool valid = it->isValid();
		it->release();
		if (found != NULL || valid)
			return found;
	}
	return NULL;
}

// the root stays in the registry for the system's life, so no reference is kept
void *
pdos_dt_root(void)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	if (root != NULL)
		root->release();
	return root;
}

// true when a nul-separated compatible list names want
static bool
dt_compat_has(IORegistryEntry *e, const char *want)
{
	OSData *d = OSDynamicCast(OSData, e->getProperty("compatible"));
	const char *str = d != NULL ? (const char *)d->getBytesNoCopy() : NULL;
	uint32_t len = d != NULL ? d->getLength() : 0;

	for (uint32_t o = 0; str != NULL && o < len; o += strnlen(str + o, len - o) + 1) {
		if (strncmp(str + o, want, len - o) == 0)
			return true;
	}
	return false;
}

// the first node in tree order with the compatible string, walked again when the registry changed underneath
void *
pdos_dt_find_compatible(const char *compat)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	IORegistryEntry *found = NULL;

	if (root == NULL)
		return NULL;
	for (int tries = 0; tries < 16 && found == NULL; tries++) {
		IORegistryIterator *it = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
		IORegistryEntry *e;

		while (it != NULL && found == NULL && (e = it->getNextObject()) != NULL) {
			if (dt_compat_has(e, compat))
				found = e;
		}
		bool valid = it != NULL && it->isValid();
		OSSafeReleaseNULL(it);
		if (valid)
			break;
	}
	root->release();
	return found;
}

// phandles were converted to native u32 cells when the loader imported the tree
void *
pdos_dt_find_phandle(uint32_t phandle)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	IORegistryEntry *found = NULL;
	if (root == NULL)
		return NULL;
	IORegistryIterator *it = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
	while (it != NULL && found == NULL) {
		IORegistryEntry *e = it->getNextObject();
		if (e == NULL)
			break;
		OSData *d = OSDynamicCast(OSData, e->getProperty("phandle"));
		if (d != NULL && d->getLength() == 4 && *(const uint32_t *)d->getBytesNoCopy() == phandle)
			found = e;
	}
	OSSafeReleaseNULL(it);
	root->release();
	return found;
}

void
pdos_dt_name(void *node, char *buf, uint32_t len)
{
	IORegistryEntry *e = (IORegistryEntry *)node;
	const char *loc;
	if (e == NULL || len == 0) {
		if (len != 0)
			buf[0] = 0;
		return;
	}
	loc = e->getLocation(gIODTPlane);
	// the loader keeps the unit address in the name already, the location only repeats it
	if (loc != NULL && strchr(e->getName(gIODTPlane), '@') == NULL)
		snprintf(buf, len, "%s@%s", e->getName(gIODTPlane), loc);
	else
		snprintf(buf, len, "%s", e->getName(gIODTPlane));
}
