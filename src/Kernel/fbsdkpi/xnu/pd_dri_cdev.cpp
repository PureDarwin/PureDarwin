/*
 * Copyright 2026, PureDarwin Project. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
// /dev/dri for a drm kext: a cloning character device, each open gets its own minor and linux file
// open, close, read, ioctl and select go to linuxkpi, mmap to xnu's pd_cdev_mmap handler

#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <miscfs/devfs/devfs.h>
#include <kern/locks.h>
#include <libkern/OSMalloc.h>
#include <IOKit/IOLib.h>

#include "pd_dri_cdev.h"

extern "C" int pd_cdev_mmap_register(int major,
	int (*fn)(dev_t dev, off_t offset, vm_size_t size, int prot, ppnum_t *pages,
		unsigned int *wimg, void (**release)(void *), void **release_arg));

// a clone minor is (open slot << 8) | drm minor, slot 0 marks the node itself
#define PD_DRI_SLOTS		1024
#define PD_DRI_SLOT(dev)	((minor(dev) >> 8) & 0xffff)
#define PD_DRI_DRM_MINOR(dev)	(minor(dev) & 0xff)

static int sMajor = -1;
static void *sSlots[PD_DRI_SLOTS];
static bool sSlotUsed[PD_DRI_SLOTS];
static lck_grp_t *sLockGroup;
static lck_mtx_t *sLock;
static const struct pd_dri_ops *sOps;

static void *
slot_cookie(dev_t dev)
{
	int slot = PD_DRI_SLOT(dev);
	return slot > 0 && slot < PD_DRI_SLOTS ? sSlots[slot] : NULL;
}

static int
dri_clone(dev_t dev, int action)
{
	int slot;

	if (action == DEVFS_CLONE_ALLOC) {
		lck_mtx_lock(sLock);
		for (slot = 1; slot < PD_DRI_SLOTS; slot++) {
			if (!sSlotUsed[slot]) {
				sSlotUsed[slot] = true;
				break;
			}
		}
		lck_mtx_unlock(sLock);
		if (slot == PD_DRI_SLOTS)
			return -1;
		return (slot << 8) | PD_DRI_DRM_MINOR(dev);
	}
	slot = PD_DRI_SLOT(dev);
	if (slot > 0 && slot < PD_DRI_SLOTS) {
		lck_mtx_lock(sLock);
		sSlotUsed[slot] = false;
		sSlots[slot] = NULL;
		lck_mtx_unlock(sLock);
	}
	return 0;
}

static int
dri_open(dev_t dev, int flags, int devtype, proc_t p)
{
	void *cookie = NULL;
	int slot = PD_DRI_SLOT(dev);
	int error;

	if (slot <= 0 || slot >= PD_DRI_SLOTS)
		return ENXIO;
	error = sOps->open(PD_DRI_DRM_MINOR(dev), flags, &cookie);
	if (error != 0)
		return error;
	sSlots[slot] = cookie;
	return 0;
}

static int
dri_close(dev_t dev, int flags, int devtype, proc_t p)
{
	void *cookie = slot_cookie(dev);

	if (cookie != NULL) {
		sOps->close(cookie);
		sSlots[PD_DRI_SLOT(dev)] = NULL;
	}
	return 0;
}

static int
dri_read(dev_t dev, struct uio *uio, int ioflag)
{
	void *cookie = slot_cookie(dev);
	char buffer[512];
	int error;

	if (cookie == NULL)
		return ENXIO;
	while (uio_resid(uio) > 0) {
		size_t want = uio_resid(uio) < (user_ssize_t)sizeof(buffer) ?
			(size_t)uio_resid(uio) : sizeof(buffer);
		ssize_t got = sOps->read(cookie, buffer, want, (ioflag & IO_NDELAY) != 0);
		if (got < 0)
			return (int)-got;
		if (got == 0)
			break;
		error = uiomove(buffer, (int)got, uio);
		if (error != 0 || (size_t)got < want)
			return error;
	}
	return 0;
}

static int
dri_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, proc_t p)
{
	void *cookie = slot_cookie(dev);

	if (cookie == NULL)
		return ENXIO;
	return sOps->ioctl(cookie, (unsigned int)cmd, data, IOCPARM_LEN(cmd));
}

static int
dri_select(dev_t dev, int which, void *wql, proc_t p)
{
	void *cookie = slot_cookie(dev);

	if (cookie == NULL)
		return 0;
	return sOps->poll(cookie, which, wql);
}

static int
dri_mmap_pages(dev_t dev, off_t offset, vm_size_t size, int prot, ppnum_t *pages,
	unsigned int *wimg, void (**release)(void *), void **release_arg)
{
	void *cookie = slot_cookie(dev);

	if (cookie == NULL)
		return ENXIO;
	return sOps->mmap_pages(cookie, offset, size, prot, pages, wimg, release, release_arg);
}

static struct cdevsw sDriCdevsw = {
	.d_open = dri_open,
	.d_close = dri_close,
	.d_read = dri_read,
	.d_write = eno_rdwrt,
	.d_ioctl = dri_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = NULL,
	.d_select = dri_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0,
};

extern "C" int
pd_dri_register(const struct pd_dri_ops *ops)
{
	if (sMajor >= 0)
		return 0;
	sOps = ops;
	sLockGroup = lck_grp_alloc_init("pd dri", LCK_GRP_ATTR_NULL);
	sLock = lck_mtx_alloc_init(sLockGroup, LCK_ATTR_NULL);
	sMajor = cdevsw_add(-1, &sDriCdevsw);
	if (sMajor < 0)
		return ENXIO;
	pd_cdev_mmap_register(sMajor, dri_mmap_pages);
	IOLog("pd_dri: character major %d\n", sMajor);
	return 0;
}

// kern.pd_dri.major and kern.pd_dri.devices, which libdrm reads to find each minor's pci device,
// one "minor busid vendor device subvendor subdevice revision" line per minor
struct pd_dri_info {
	bool used;
	char busid[32];
	uint16_t vendor, device, subvendor, subdevice;
	uint8_t revision;
};
static struct pd_dri_info sInfo[256];

static int
dri_sysctl_devices(__unused struct sysctl_oid *oidp, __unused void *arg1,
	__unused int arg2, struct sysctl_req *req)
{
	char *text = (char *)IOMallocZero(256 * 64);
	size_t len = 0;
	int error;

	if (text == NULL)
		return ENOMEM;
	for (int i = 0; i < 256; i++) {
		if (!sInfo[i].used)
			continue;
		len += snprintf(text + len, 256 * 64 - len, "%d %s 0x%04x 0x%04x 0x%04x 0x%04x 0x%02x\n",
		    i, sInfo[i].busid, sInfo[i].vendor, sInfo[i].device, sInfo[i].subvendor,
		    sInfo[i].subdevice, sInfo[i].revision);
	}
	error = SYSCTL_OUT(req, text, len + 1);
	IOFree(text, 256 * 64);
	return error;
}

SYSCTL_DECL(_kern);
SYSCTL_NODE(_kern, OID_AUTO, pd_dri, CTLFLAG_RD | CTLFLAG_LOCKED, 0, "drm devices");
SYSCTL_INT(_kern_pd_dri, OID_AUTO, major, CTLFLAG_RD | CTLFLAG_LOCKED, &sMajor, 0, "dri character major");
SYSCTL_PROC(_kern_pd_dri, OID_AUTO, devices, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, dri_sysctl_devices, "A", "drm minors and their pci devices");

static bool sSysctls;

static void
dri_register_sysctls(void)
{
	if (sSysctls)
		return;
	sysctl_register_oid(&sysctl__kern_pd_dri);
	sysctl_register_oid(&sysctl__kern_pd_dri_major);
	sysctl_register_oid(&sysctl__kern_pd_dri_devices);
	sSysctls = true;
}

// a node per drm minor: dri/card0, dri/renderD128 and so on
extern "C" int
pd_dri_make_node(int drm_minor, const char *name, const struct pd_dri_pci *pci)
{
	if (sMajor < 0)
		return ENXIO;
	if (pci != NULL && drm_minor >= 0 && drm_minor < 256) {
		struct pd_dri_info *info = &sInfo[drm_minor];

		snprintf(info->busid, sizeof(info->busid), "pci:%04x:%02x:%02x.%u", pci->domain,
		    pci->bus, pci->dev, pci->func);
		info->vendor = pci->vendor;
		info->device = pci->device;
		info->subvendor = pci->subvendor;
		info->subdevice = pci->subdevice;
		info->revision = pci->revision;
		info->used = true;
		dri_register_sysctls();
		IOLog("pd_dri: minor %d is %s, %04x:%04x\n", drm_minor, info->busid, info->vendor,
		    info->device);
	}
	if (devfs_make_node_clone(makedev(sMajor, drm_minor & 0xff), DEVFS_CHAR, 0, 0, 0666,
	    dri_clone, "dri/%s", name) == NULL)
		return ENOMEM;
	IOLog("pd_dri: /dev/dri/%s\n", name);
	return 0;
}
