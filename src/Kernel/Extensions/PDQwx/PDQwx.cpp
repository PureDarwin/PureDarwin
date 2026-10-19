// SPDX-License-Identifier: MIT
// Qualcomm WCN6855 (QCNFA765) PCIe 802.11ax, OpenBSD's qwx(4) on IO80211Family, with the pci and bus_dma calls it makes

#include "IO80211Controller.h"
#include "pd_qwx_glue.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/kpi_mbuf.h>

#include "pd_qwx_compat.h"

extern "C" {
void *pd80211_malloc(size_t, int, int);
void pd80211_free(void *, int, size_t);
void pd80211_lock(void);
void pd80211_unlock(void);
vnode_t vfs_rootvnode(void);
}

#define QWX_FIRMWARE_DIR	"/Library/Firmware/qwx"
#define QWX_ROOT_WAIT_S		90
#define QWX_MAX_INTRS		4
#define QWX_MAX_DEVICES		4

class PDQwx;

struct pd_qwx_dma {
	IOPCIDevice	*pci;
};

struct pd_qwx_intr {
	PDQwx		*owner;
	IOInterruptEventSource *source;
	int		(*fn)(void *);
	void		*arg;
};

class PDQwx : public IO80211Controller
{
	OSDeclareDefaultStructors(PDQwx)

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual const OSString *newVendorString() const APPLE_KEXT_OVERRIDE;
	virtual const OSString *newModelString() const APPLE_KEXT_OVERRIDE;

	IOPCIDevice *fPCI;
	IOWorkLoop *fIntrLoop;
	IOMemoryMap *fBar;
	struct pd_qwx_intr fIntrs[QWX_MAX_INTRS];

private:
	void teardown();

	struct pd_qwx_dma fDma;
	struct device *fDev;
	bool fAttached;
};

#define super IO80211Controller
OSDefineMetaClassAndStructors(PDQwx, IO80211Controller)

// the pci tag is the IOPCIDevice, this finds the driver behind it
static struct {
	IOPCIDevice *pci;
	PDQwx *owner;
} sDevices[QWX_MAX_DEVICES];
static IOLock *sDevicesLock;

static void setOwner(IOPCIDevice *pci, PDQwx *owner)
{
	if (sDevicesLock == NULL) {
		IOLock *lock = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&sDevicesLock))
			IOLockFree(lock);
	}
	IOLockLock(sDevicesLock);
	for (int i = 0; i < QWX_MAX_DEVICES; i++) {
		if (owner != NULL && sDevices[i].pci == NULL) {
			sDevices[i].pci = pci;
			sDevices[i].owner = owner;
			break;
		}
		if (owner == NULL && sDevices[i].pci == pci) {
			sDevices[i].pci = NULL;
			sDevices[i].owner = NULL;
			break;
		}
	}
	IOLockUnlock(sDevicesLock);
}

static PDQwx *ownerOf(void *pci)
{
	PDQwx *owner = NULL;

	if (sDevicesLock == NULL)
		return NULL;
	IOLockLock(sDevicesLock);
	for (int i = 0; i < QWX_MAX_DEVICES; i++) {
		if (sDevices[i].pci == pci)
			owner = sDevices[i].owner;
	}
	IOLockUnlock(sDevicesLock);
	return owner;
}

static bool rootMounted()
{
	vnode_t root = vfs_rootvnode();

	if (root == NULLVP)
		return false;
	vnode_put(root);
	return true;
}

// firmware comes off the root file system, a driver starting before it waits
static bool waitForRoot()
{
	for (int i = 0; i < QWX_ROOT_WAIT_S * 10 && !rootMounted(); i++)
		IOSleep(100);
	return rootMounted();
}

bool PDQwx::start(IOService *provider)
{
	struct pci_attach_args pa;
	char variant[32];

	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	if (!fPCI->open(this)) {
		IOLog("PDQwx: cannot open the pci device\n");
		return false;
	}
	fIntrLoop = IOWorkLoop::workLoop();
	if (fIntrLoop == NULL) {
		fPCI->close(this);
		return false;
	}
	setOwner(fPCI, this);

	fDma.pci = fPCI;
	bzero(&pa, sizeof(pa));
	pa.pa_pc = fPCI;
	pa.pa_tag = fPCI;
	pa.pa_dmat = &fDma;
	pa.pa_id = fPCI->configRead32(kIOPCIConfigVendorID);
	IOLog("PDQwx: pci %04x:%04x\n", pa.pa_id & 0xffff, pa.pa_id >> 16);

	fDev = pd_qwx_alloc("qwx0");
	if (fDev == NULL || !pd_qwx_match(&pa) || pd_qwx_attach(fDev, &pa) != 0) {
		IOLog("PDQwx: the pci side did not attach\n");
		teardown();
		return false;
	}
	fAttached = true;
	if (PE_parse_boot_argn("qwxvariant", variant, sizeof(variant)) && variant[0] != '\0') {
		IOLog("PDQwx: board variant %s\n", variant);
		pd_qwx_set_variant(fDev, variant);
	}

	// qwx reads the mac address from the running firmware, so the radio needs the root fs
	if (!waitForRoot() || pd_qwx_attach_hook(fDev) != 0) {
		IOLog("PDQwx: firmware did not come up, see %s\n", QWX_FIRMWARE_DIR);
		teardown();
		return false;
	}
	if (!attach80211(pd_qwx_ic(fDev))) {
		teardown();
		return false;
	}
	registerService();
	return true;
}

void PDQwx::teardown()
{
	if (fAttached)
		pd_qwx_detach(fDev);
	fAttached = false;
	if (fDev != NULL)
		pd_qwx_free(fDev);
	fDev = NULL;
	for (int i = 0; i < QWX_MAX_INTRS; i++) {
		if (fIntrs[i].source == NULL)
			continue;
		fIntrs[i].source->disable();
		fIntrLoop->removeEventSource(fIntrs[i].source);
		OSSafeReleaseNULL(fIntrs[i].source);
	}
	OSSafeReleaseNULL(fBar);
	OSSafeReleaseNULL(fIntrLoop);
	if (fPCI != NULL) {
		setOwner(fPCI, NULL);
		fPCI->close(this);
	}
}

void PDQwx::stop(IOService *provider)
{
	detach80211();
	teardown();
	super::stop(provider);
}

const OSString *PDQwx::newVendorString() const
{
	return OSString::withCString("Qualcomm");
}

const OSString *PDQwx::newModelString() const
{
	return OSString::withCString("WCN6855 802.11ax");
}

// the no-hardware check, kept out of the way unless the qwxfwtest boot-arg asks for it
class PDQwxSelfTest : public IOService
{
	OSDeclareDefaultStructors(PDQwxSelfTest)

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;

private:
	static void run(void *arg, wait_result_t wr);
};

OSDefineMetaClassAndStructors(PDQwxSelfTest, IOService)

void PDQwxSelfTest::run(void *arg, wait_result_t wr)
{
	int which = (int)(uintptr_t)arg;

	if (!waitForRoot()) {
		IOLog("qwxtest: no root file system\n");
		return;
	}
	// 1 checks wcn6855 hw2.0, 2 checks hw2.1, 3 both
	if (which & 1)
		pd_qwx_fwcheck(0);
	if (which & 2)
		pd_qwx_fwcheck(1);
}

bool PDQwxSelfTest::start(IOService *provider)
{
	int which = 0;
	thread_t thread;

	if (!PE_parse_boot_argn("qwxfwtest", &which, sizeof(which)) || which == 0)
		return false;
	if (!IOService::start(provider))
		return false;
	IOLog("qwxtest: firmware check %d queued until the root mount\n", which);
	if (kernel_thread_start(&PDQwxSelfTest::run, (void *)(uintptr_t)which, &thread) == KERN_SUCCESS)
		thread_deallocate(thread);
	return true;
}

// the openbsd calls qwx makes

extern "C" {

int loadfirmware(const char *name, u_char **bufp, size_t *sizep)
{
	char path[MAXPATHLEN];
	vfs_context_t ctx;
	vnode_t vp = NULLVP;
	struct vnode_attr va;
	u_char *buf = NULL;
	size_t len = 0;
	int error, resid = 0;

	if (!rootMounted())
		return ENOENT;
	snprintf(path, sizeof(path), "%s/%s", QWX_FIRMWARE_DIR, name);
	ctx = vfs_context_create(NULL);
	error = vnode_open(path, FREAD, 0, 0, &vp, ctx);
	if (error != 0) {
		vfs_context_rele(ctx);
		return error;
	}

	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	error = vnode_getattr(vp, &va, ctx);
	if (error == 0 && (va.va_data_size == 0 || va.va_data_size > 32 * 1024 * 1024))
		error = EFBIG;
	if (error == 0) {
		len = (size_t)va.va_data_size;
		buf = (u_char *)pd80211_malloc(len, 2, 0);
		if (buf == NULL)
			error = ENOMEM;
	}
	if (error == 0) {
		error = vn_rdwr(UIO_READ, vp, (caddr_t)buf, (int)len, 0, UIO_SYSSPACE, 0,
		    vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
		if (error == 0 && resid != 0)
			error = EIO;
	}

	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (error != 0) {
		if (buf != NULL)
			pd80211_free(buf, 2, len);
		return error;
	}
	*bufp = buf;
	*sizep = len;
	return 0;
}

// config space, the tag is the IOPCIDevice

pcireg_t pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	return ((IOPCIDevice *)tag)->configRead32(reg);
}

void pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t val)
{
	((IOPCIDevice *)tag)->configWrite32(reg, val);
}

int pci_get_capability(pci_chipset_tag_t pc, pcitag_t tag, int capid, int *offp, pcireg_t *valp)
{
	IOPCIDevice *pci = (IOPCIDevice *)tag;
	UInt8 off = 0;

	if (pci->findPCICapability((UInt8)capid, &off) == 0 || off == 0)
		return 0;
	if (offp != NULL)
		*offp = off;
	if (valp != NULL)
		*valp = pci->configRead32(off);
	return 1;
}

pcireg_t pci_mapreg_type(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	return ((IOPCIDevice *)tag)->configRead32(reg) & PCI_MAPREG_MEM_TYPE_64BIT;
}

int pci_mapreg_map(struct pci_attach_args *pa, int reg, pcireg_t type, int flags,
    bus_space_tag_t *tagp, bus_space_handle_t *handlep, bus_addr_t *basep,
    bus_size_t *sizep, bus_size_t maxsize)
{
	IOPCIDevice *pci = (IOPCIDevice *)pa->pa_tag;
	PDQwx *owner = ownerOf(pci);
	IOMemoryMap *map;

	if (owner == NULL)
		return ENXIO;
	map = pci->mapDeviceMemoryWithRegister((UInt8)reg, kIOMapInhibitCache);
	if (map == NULL)
		return ENOMEM;
	OSSafeReleaseNULL(owner->fBar);
	owner->fBar = map;
	pci->setMemoryEnable(true);
	if (tagp != NULL)
		*tagp = 0;
	if (handlep != NULL)
		*handlep = (bus_space_handle_t)map->getVirtualAddress();
	if (basep != NULL)
		*basep = map->getPhysicalAddress();
	if (sizep != NULL)
		*sizep = map->getLength();
	return 0;
}

int pci_matchbyid(struct pci_attach_args *pa, const struct pci_matchid *ids, int n)
{
	for (int i = 0; i < n; i++) {
		if (PCI_VENDOR(pa->pa_id) == ids[i].pm_vid && PCI_PRODUCT(pa->pa_id) == ids[i].pm_pid)
			return 1;
	}
	return 0;
}

int pci_set_powerstate(pci_chipset_tag_t pc, pcitag_t tag, int state)
{
	IOPCIDevice *pci = (IOPCIDevice *)tag;
	UInt8 off = 0;

	if (pci->findPCICapability(0x01, &off) == 0 || off == 0)
		return 0;
	pci->configWrite16(off + 4, (pci->configRead16(off + 4) & ~3) | (state & 3));
	IODelay(10000);
	return 0;
}

// one msi vector, qwx falls back to it when the multi vector setup is refused

int pci_intr_enable_msivec(struct pci_attach_args *pa, int n)
{
	return 1;
}

int pci_intr_map_msi(struct pci_attach_args *pa, pci_intr_handle_t *ih)
{
	IOPCIDevice *pci = (IOPCIDevice *)pa->pa_tag;

	for (int i = 0; ; i++) {
		int type = 0;

		if (pci->getInterruptType(i, &type) != kIOReturnSuccess)
			break;
		if (type & kIOInterruptTypePCIMessaged) {
			ih->ih_index = i;
			return 0;
		}
	}
	return 1;
}

int pci_intr_map_msivec(struct pci_attach_args *pa, int vec, pci_intr_handle_t *ih)
{
	return vec == 0 ? pci_intr_map_msi(pa, ih) : 1;
}

int pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ih)
{
	ih->ih_index = 0;
	return 0;
}

const char *pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	return "msi";
}

// openbsd runs the handler at IPL_NET, here it runs on the work loop under the stack lock
static void intrAction(OSObject *target, IOInterruptEventSource *src, int count)
{
	for (int i = 0; i < QWX_MAX_INTRS; i++) {
		PDQwx *owner = (PDQwx *)target;
		struct pd_qwx_intr *in = &owner->fIntrs[i];

		if (in->source != src)
			continue;
		pd80211_lock();
		in->fn(in->arg);
		pd80211_unlock();
	}
}

void *pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih, int level,
    int (*fn)(void *), void *arg, const char *name)
{
	IOPCIDevice *pci = (IOPCIDevice *)pc;
	PDQwx *owner = ownerOf(pci);
	struct pd_qwx_intr *in = NULL;

	if (owner == NULL)
		return NULL;
	for (int i = 0; i < QWX_MAX_INTRS && in == NULL; i++) {
		if (owner->fIntrs[i].source == NULL)
			in = &owner->fIntrs[i];
	}
	if (in == NULL)
		return NULL;
	in->owner = owner;
	in->fn = fn;
	in->arg = arg;
	in->source = IOInterruptEventSource::interruptEventSource(owner, intrAction, pci, ih.ih_index);
	if (in->source == NULL || owner->fIntrLoop->addEventSource(in->source) != kIOReturnSuccess) {
		OSSafeReleaseNULL(in->source);
		return NULL;
	}
	pci->setBusMasterEnable(true);
	in->source->enable();
	return in;
}

void pci_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
{
	struct pd_qwx_intr *in = (struct pd_qwx_intr *)cookie;

	if (in == NULL || in->source == NULL)
		return;
	in->source->disable();
	in->owner->fIntrLoop->removeEventSource(in->source);
	OSSafeReleaseNULL(in->source);
}

// bus_dma over IODMACommand, which also bounces what the 32-bit openbsd tag cannot reach

#define QWX_DMA_BITS	32

int bus_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegs, bus_size_t maxsegsz,
    bus_size_t boundary, int flags, bus_dmamap_t *mapp)
{
	size_t len = sizeof(struct bus_dmamap) + (nsegs - 1) * sizeof(bus_dma_segment_t);
	bus_dmamap_t map = (bus_dmamap_t)IOMallocZero(len);

	if (map == NULL)
		return ENOMEM;
	map->_dm_maxsegs = nsegs;
	map->_dm_size = size;
	*mapp = map;
	return 0;
}

void bus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	IODMACommand *cmd = (IODMACommand *)map->_dm_cmd;
	IOMemoryDescriptor *md = (IOMemoryDescriptor *)map->_dm_md;

	if (cmd != NULL) {
		cmd->clearMemoryDescriptor(true);
		cmd->release();
	}
	if (md != NULL)
		md->release();
	map->_dm_cmd = NULL;
	map->_dm_md = NULL;
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
}

void bus_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	bus_dmamap_unload(t, map);
	IOFree(map, sizeof(struct bus_dmamap) + (map->_dm_maxsegs - 1) * sizeof(bus_dma_segment_t));
}

// md is retained by the map from here on
static int loadDescriptor(bus_dmamap_t map, IOMemoryDescriptor *md, bus_size_t len)
{
	IODMACommand *cmd;
	IODMACommand::Segment64 segs[8];
	UInt32 nsegs = map->_dm_maxsegs < 8 ? map->_dm_maxsegs : 8;
	UInt64 offset = 0;

	cmd = IODMACommand::withSpecification(kIODMACommandOutputHost64, QWX_DMA_BITS, 0,
	    IODMACommand::kMapped, 0, 1);
	if (cmd == NULL) {
		md->release();
		return ENOMEM;
	}
	if (cmd->setMemoryDescriptor(md, true) != kIOReturnSuccess ||
	    cmd->genIOVMSegments(&offset, segs, &nsegs) != kIOReturnSuccess) {
		cmd->release();
		md->release();
		return ENOMEM;
	}
	// more segments than the map has is EFBIG, which qwx answers with m_defrag
	if (offset < len) {
		cmd->clearMemoryDescriptor(true);
		cmd->release();
		md->release();
		return EFBIG;
	}
	for (UInt32 i = 0; i < nsegs; i++) {
		map->dm_segs[i].ds_addr = segs[i].fIOVMAddr;
		map->dm_segs[i].ds_len = segs[i].fLength;
	}
	map->dm_nsegs = (int)nsegs;
	map->dm_mapsize = len;
	map->_dm_cmd = cmd;
	map->_dm_md = md;
	return 0;
}

int bus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf, bus_size_t len, void *p, int flags)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)buf, len,
	    kIODirectionInOut, kernel_task);

	if (md == NULL)
		return ENOMEM;
	return loadDescriptor(map, md, len);
}

int bus_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m, int flags)
{
	mbuf_t mb = (mbuf_t)m;
	IOMemoryDescriptor *md;

	if (mbuf_next(mb) != NULL)
		return EFBIG;
	md = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)mbuf_data(mb), mbuf_len(mb),
	    kIODirectionInOut, kernel_task);
	if (md == NULL)
		return ENOMEM;
	return loadDescriptor(map, md, mbuf_len(mb));
}

int bus_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map, bus_dma_segment_t *segs, int nsegs,
    bus_size_t size, int flags)
{
	IOMemoryDescriptor *md = (IOMemoryDescriptor *)segs[0]._ds_mem;

	if (md == NULL || nsegs != 1)
		return EINVAL;
	md->retain();
	return loadDescriptor(map, md, size);
}

void bus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t off, bus_size_t len, int ops)
{
	IODMACommand *cmd = (IODMACommand *)map->_dm_cmd;

	if (cmd == NULL)
		return;
	if (ops & (BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD))
		cmd->synchronize(kIODirectionOut);
	if (ops & BUS_DMASYNC_POSTREAD)
		cmd->synchronize(kIODirectionIn);
	OSSynchronizeIO();
}

int bus_dmamem_alloc_range(bus_dma_tag_t t, bus_size_t size, bus_size_t align, bus_size_t boundary,
    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags, bus_addr_t low, bus_addr_t high)
{
	IOBufferMemoryDescriptor *bmd;
	mach_vm_address_t mask = high;

	// an aligned physical mask is how iokit asks for alignment
	if (align > 1)
		mask &= ~(mach_vm_address_t)(align - 1);
	bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous, size, mask);
	if (bmd == NULL)
		return ENOMEM;
	if (bmd->prepare() != kIOReturnSuccess) {
		bmd->release();
		return ENOMEM;
	}
	bzero(bmd->getBytesNoCopy(), size);
	segs[0].ds_addr = bmd->getPhysicalAddress();
	segs[0].ds_len = size;
	segs[0]._ds_mem = bmd;
	*rsegs = 1;
	return 0;
}

int bus_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t align, bus_size_t boundary,
    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags)
{
	return bus_dmamem_alloc_range(t, size, align, boundary, segs, nsegs, rsegs, flags, 0,
	    (1ULL << QWX_DMA_BITS) - 1);
}

void bus_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
	IOBufferMemoryDescriptor *bmd = (IOBufferMemoryDescriptor *)segs[0]._ds_mem;

	if (bmd == NULL)
		return;
	bmd->complete();
	bmd->release();
	segs[0]._ds_mem = NULL;
}

int bus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap, int flags)
{
	IOBufferMemoryDescriptor *bmd = (IOBufferMemoryDescriptor *)segs[0]._ds_mem;

	if (bmd == NULL)
		return EINVAL;
	*kvap = (caddr_t)bmd->getBytesNoCopy();
	return 0;
}

void bus_dmamem_unmap(bus_dma_tag_t t, caddr_t kva, size_t size)
{
}

}
