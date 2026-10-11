#ifndef _PD_PVR_UAPI_H
#define _PD_PVR_UAPI_H

// the PDPowerVR user client: selectors 0x40-0x4d are the powervr drm ioctls by number, with the ioctl's argument struct as structure input and output.
// the rest stand in for the generic drm ioctls mesa uses (gem close, syncobjs). a buffer is mapped with IOConnectMapMemory64 using the GET_BO_MMAP_OFFSET offset as the memory type

#include <stdint.h>

#define PD_PVR_SERVICE_CLASS		"PDPowerVR"

#define PD_PVR_SEL_IOCTL_FIRST		0x40
#define PD_PVR_SEL_IOCTL_LAST		0x4d
#define PD_PVR_SEL_GEM_CLOSE		0x100	// scalar in: handle
#define PD_PVR_SEL_SYNCOBJ_CREATE	0x101	// scalar in: flags; scalar out: handle
#define PD_PVR_SEL_SYNCOBJ_DESTROY	0x102	// scalar in: handle
#define PD_PVR_SEL_SYNCOBJ_WAIT		0x103	// struct pd_pvr_syncobj_wait in and out
#define PD_PVR_SEL_SYNCOBJ_RESET	0x104	// scalar in: handle
#define PD_PVR_SEL_SYNCOBJ_TRANSFER 0x106 // scalar: src, dst, join
#define PD_PVR_SEL_SYNCOBJ_SIGNAL	0x105	// scalar in: handle

// scanout: dumb buffers are contiguous, prime moves a buffer between files by token, and a flip puts
// a buffer on the display once the gpu work queued before it is done
#define PD_PVR_SEL_DUMB_CREATE		0x110	// scalar in: size, flags; scalar out: handle, contiguous
#define PD_PVR_SEL_PRIME_EXPORT		0x111	// scalar in: handle; scalar out: token, size
#define PD_PVR_SEL_PRIME_IMPORT		0x112	// scalar in: token; scalar out: handle, size
#define PD_PVR_SEL_PRIME_RELEASE	0x113	// scalar in: token
#define PD_PVR_SEL_DISPLAY_INFO		0x114	// struct out: pd_pvr_display_info
#define PD_PVR_SEL_FLIP			0x115	// scalar in: handle (0 restores the console), offset, pitch;
						// scalar out: vblank sequence, time ns

// a failing call returns the linux errno in the low bits of a vendor-specific iokit error
#define PD_PVR_ERR_BASE			0xe3ff8000u	// sys_iokit | sub_iokit_vendor_specific
#define PD_PVR_ERR_FROM_ERRNO(e)	(PD_PVR_ERR_BASE | ((e) & 0x3fff))
#define PD_PVR_ERRNO(kr)		((((uint32_t)(kr)) & 0xffffc000u) == PD_PVR_ERR_BASE ? \
					 (int)((kr) & 0x3fff) : -1)

#define PD_PVR_SYNCOBJ_CREATE_SIGNALED	(1u << 0)
#define PD_PVR_SYNCOBJ_WAIT_ALL		(1u << 0)

struct pd_pvr_syncobj_wait {
	uint64_t handles;	// user pointer to uint32_t handles
	uint32_t count;
	uint32_t flags;
	uint64_t timeout_us;	// relative
	uint32_t first_signaled;
	uint32_t pad;
};

// a dumb buffer the task maps write-back cached, it cleans what it draws before a flip or scanout
#define PD_PVR_DUMB_CPU_CACHED		(1u << 0)

struct pd_pvr_display_info {
	uint32_t width, height;
	uint32_t pitch;			// of the console buffer
	uint32_t format;		// drm fourcc
	uint32_t refresh_mhz;
	uint32_t pad;
};

#endif
