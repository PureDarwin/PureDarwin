#ifndef _PVR_DRM_MIN_H
#define _PVR_DRM_MIN_H

// the two powervr uapi calls pdvklife needs, laid out as in the kernel's pvr_drm.h

#include <stdint.h>
#include <drm.h>

#define PVR_BO_ALLOW_CPU_USERSPACE_ACCESS	(1ULL << 2)

struct pvr_create_bo {
	uint64_t size;
	uint32_t handle;
	uint32_t pad;
	uint64_t flags;
};

struct pvr_mmap_offset {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
};

#define PVR_IOCTL_CREATE_BO		DRM_IOWR(DRM_COMMAND_BASE + 0x01, struct pvr_create_bo)
#define PVR_IOCTL_GET_BO_MMAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + 0x02, struct pvr_mmap_offset)

#endif
