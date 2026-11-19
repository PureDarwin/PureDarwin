// PDNVUVM.cpp - the IOKit side of nvidia-uvm: one instance per GPU PDNVLoader has brought up, and /dev/nvidia-uvm
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSAtomic.h>

extern "C" {
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/filio.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <miscfs/devfs/devfs.h>
}

#include "../../fbsdkpi/pd/pd_fbsd_attach.h"
#include "../../fbsdkpi/xnu/pd_fbsd_xnu.h"
#include "../../fbsdkpi/pd/pd_fbsd_driver.h"
#include "pd_uvm_ioctl.h"

extern "C" {
// PDNVLoader's, which this kext links against
uint32_t pdnv_gpu_count(void);
void *pdnv_gpu_pci_device(uint32_t i);
// pd_uvm_glue.c, the linuxkpi side
int pd_uvm_claimed_gpu(void);
int pd_uvm_open(unsigned int minor, int flags, void **cookie);
void pd_uvm_close(void *cookie);
int pd_uvm_ioctl(void *cookie, unsigned int cmd, unsigned long uarg);
int pd_uvm_mmap(void *filp, unsigned long offset, unsigned long size, int prot, uint32_t *pages,
    void (**release)(void *), void **release_arg);
// the kernel's: a driver's mmap for its character devices, the pages listed at mmap time (bsd/kern/kern_mman.c)
int pd_cdev_mmap_register(int major, int (*fn)(dev_t, off_t, vm_size_t, int, ppnum_t *, unsigned int *,
    void (**)(void *), void **));
}

// a clone minor is (open slot << 8) | node, node 0 is /dev/nvidia-uvm and 1 /dev/nvidia-uvm-tools, as on Linux
#define PD_UVM_SLOTS 1024
#define SLOT(dev) ((minor(dev) >> 8) & 0xffff)
#define NODE(dev) (minor(dev) & 0xff)

static IOLock *sLock;
static int sMajor = -1;
static void *sFiles[PD_UVM_SLOTS];
static bool sSlotUsed[PD_UVM_SLOTS];

static int
uvm_clone(dev_t dev, int action)
{
	int slot;

	if (action == DEVFS_CLONE_ALLOC) {
		IOLockLock(sLock);
		for (slot = 1; slot < PD_UVM_SLOTS && sSlotUsed[slot]; slot++)
			;
		if (slot < PD_UVM_SLOTS)
			sSlotUsed[slot] = true;
		IOLockUnlock(sLock);
		return slot < PD_UVM_SLOTS ? (slot << 8) | NODE(dev) : -1;
	}
	slot = SLOT(dev);
	if (slot > 0 && slot < PD_UVM_SLOTS) {
		IOLockLock(sLock);
		sSlotUsed[slot] = false;
		sFiles[slot] = NULL;
		IOLockUnlock(sLock);
	}
	return 0;
}

static void *
slot_file(dev_t dev)
{
	int slot = SLOT(dev);

	return slot > 0 && slot < PD_UVM_SLOTS ? sFiles[slot] : NULL;
}

static int
uvm_open(dev_t dev, int flags, int devtype, struct proc *p)
{
	int slot = SLOT(dev), err;
	void *f = NULL;

	(void)devtype;
	(void)p;
	if (slot <= 0 || slot >= PD_UVM_SLOTS)
		return ENXIO;
	err = pd_uvm_open(NODE(dev), flags, &f);
	if (err != 0)
		return err;
	IOLockLock(sLock);
	sFiles[slot] = f;
	IOLockUnlock(sLock);
	return 0;
}

static int
uvm_close(dev_t dev, int flags, int devtype, struct proc *p)
{
	void *f = slot_file(dev);

	(void)flags;
	(void)devtype;
	(void)p;
	if (f != NULL)
		pd_uvm_close(f);
	return 0;
}

static int
uvm_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	void *f = slot_file(dev);
	struct pd_uvm_ioctl *u = (struct pd_uvm_ioctl *)data;

	(void)fflag;
	(void)p;
	if (f == NULL)
		return ENOTTY;
	// fcntl's O_NONBLOCK and O_ASYNC reach a device as these, Linux never hands them to the driver
	if (cmd == FIONBIO || cmd == FIOASYNC)
		return 0;
	if (cmd != PD_UVM_IOCTL)
		return ENOTTY;
	return pd_uvm_ioctl(f, u->cmd, (unsigned long)u->arg);
}

// the linux file behind one of the calling process's descriptors, if it is open on /dev/nvidia-uvm or -tools: what
// nvidia-uvm's fget asks for when a request names another UVM descriptor (UVM_MM_INITIALIZE)
extern "C" void *
pd_uvm_file_for_fd(int fd)
{
	vnode_t vp = NULLVP;
	dev_t dev;

	if (sMajor < 0 || file_vnode(fd, &vp) != 0)
		return NULL;
	dev = vnode_vtype(vp) == VCHR ? vnode_specrdev(vp) : NODEV;
	file_drop(fd);
	if (dev == NODEV || major(dev) != sMajor)
		return NULL;
	return slot_file(dev);
}

// an mmap of /dev/nvidia-uvm: nvidia-uvm's own mmap on the descriptor's linux file, its inserted pages the mapping
static int
uvm_mmap_pages(dev_t dev, off_t offset, vm_size_t size, int prot, ppnum_t *pages, unsigned int *wimg,
    void (**release)(void *), void **release_arg)
{
	void *f = slot_file(dev);

	if (f == NULL)
		return ENXIO;
	// the pages are system memory, mapped cached as on Linux
	*wimg = 0x2;    // VM_WIMG_DEFAULT
	return pd_uvm_mmap(f, (unsigned long)offset, (unsigned long)size, prot, pages, release, release_arg);
}

static struct cdevsw sCdevsw = {
	.d_open = uvm_open,
	.d_close = uvm_close,
	.d_read = eno_rdwrt,
	.d_write = eno_rdwrt,
	.d_ioctl = uvm_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = eno_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

// /dev/nvidia-uvm and /dev/nvidia-uvm-tools, once nvidia-uvm's init has run
static int
uvm_dev_init(void)
{
	sMajor = cdevsw_add(-1, &sCdevsw);
	if (sMajor < 0)
		return ENXIO;
	pd_cdev_mmap_register(sMajor, uvm_mmap_pages);
	if (devfs_make_node_clone(makedev(sMajor, 0), DEVFS_CHAR, 0, 0, 0666, uvm_clone, "nvidia-uvm") == NULL ||
	    devfs_make_node_clone(makedev(sMajor, 1), DEVFS_CHAR, 0, 0, 0666, uvm_clone, "nvidia-uvm-tools") == NULL)
		return ENXIO;
	return 0;
}

class PDNVUVM : public IOService {
	OSDeclareDefaultStructors(PDNVUVM)

public:
	bool start(IOService *provider) override;
};

OSDefineMetaClassAndStructors(PDNVUVM, IOService)

// one start at a time: nvidia-uvm comes up with the first GPU, then takes the others' pci_devs
static IOLock *sStartLock;

bool
PDNVUVM::start(IOService *provider)
{
	if (!IOService::start(provider))
		return false;
	pdos_xnu_init();

	// the provider is PDNVGPU, registered once its GPU is up, on the IOPCIDevice it holds open
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider->getProvider());
	uint32_t n = pdnv_gpu_count(), i;
	for (i = 0; i < n && pdnv_gpu_pci_device(i) != pci; i++)
		;
	if (pci == NULL || i == n) {
		IOLog("PDNVUVM: %s is not a GPU PDNVLoader drives\n", provider->getName());
		return false;
	}

	if (sStartLock == NULL) {
		IOLock *lock = IOLockAlloc();
		if (!OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&sStartLock))
			IOLockFree(lock);
	}
	IOLockLock(sStartLock);
	// the first attach runs this kext's sysinits: the glue, then nvidia-uvm's own init
	// the shared layer runs this driver's sysinits and probes with its own record, one attach at a time
	pd_fbsd_driver_enter(pd_fbsd_driver_record());
	void *attached = pd_fbsd_attach_pci_device(pci);
	pd_fbsd_driver_leave();
	bool found = attached != NULL && pd_uvm_claimed_gpu();
	int err = 0;
	if (found && sMajor < 0) {
		sLock = IOLockAlloc();
		err = sLock != NULL ? uvm_dev_init() : ENOMEM;
	}
	IOLockUnlock(sStartLock);
	if (!found) {
		IOLog("PDNVUVM: GPU %u has no linux pci device\n", i);
		return false;
	}
	IOLog("PDNVUVM: GPU %u attached, /dev/nvidia-uvm %s\n", i, err == 0 ? "ready" : "not made");
	registerService();
	return true;
}
