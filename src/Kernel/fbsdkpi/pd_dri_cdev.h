// SPDX-License-Identifier: MIT
// the contract between the /dev/dri character device (PDDRMDri.kext) and the
// linuxkpi file operations of each drm kext
#ifndef _PD_DRI_CDEV_H_
#define _PD_DRI_CDEV_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// WIMG cache modes for mmap_pages, the x86 values of xnu's VM_WIMG_*
#define PD_DRI_WIMG_DEFAULT	0x2	// cached, coherent
#define PD_DRI_WIMG_WCOMB	0x6	// write combined
#define PD_DRI_WIMG_IO		0x7	// uncached, guarded

struct pd_dri_ops {
	// opens a linux file on drm minor (card N or renderD N), cookie is the file
	int (*open)(int drm_minor, int flags, void **cookie);
	void (*close)(void *cookie);
	// drm events, a negative errno or the bytes read
	long (*read)(void *cookie, void *buffer, size_t size, int nonblock);
	// data is the kernel copy of the ioctl argument, len its size
	int (*ioctl)(void *cookie, unsigned int cmd, void *data, size_t len);
	// select: nonzero when ready, otherwise records the waiter
	int (*poll)(void *cookie, int which, void *wql);
	// the physical pages behind a gem mmap offset, and their WIMG cache mode.
	// release, if set, runs once the last mapping of those pages is gone
	int (*mmap_pages)(void *cookie, off_t offset, size_t size, int prot,
		uint32_t *pages, unsigned int *wimg, void (**release)(void *), void **release_arg);
};

// the pci device behind a drm minor, for libdrm's device lookup
struct pd_dri_pci {
	uint32_t domain;
	uint8_t bus, dev, func, revision;
	uint16_t vendor, device, subvendor, subdevice;
};

// a device tree device behind a drm minor: its node name and first compatible string
struct pd_dri_platform {
	char fullname[64];
	char compatible[64];
};

int pd_dri_register(const struct pd_dri_ops *ops);
// both live in PDDRMDri.kext, shared by every drm kext
int pd_dri_make_node(const struct pd_dri_ops *ops, int drm_minor, const char *name,
	const struct pd_dri_pci *pci);
int pd_dri_make_platform_node(const struct pd_dri_ops *ops, int drm_minor, const char *name,
	const struct pd_dri_platform *plat);

#ifdef __cplusplus
}
#endif

#endif
