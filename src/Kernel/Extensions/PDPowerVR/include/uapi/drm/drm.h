#ifndef _PD_UAPI_DRM_H
#define _PD_UAPI_DRM_H

// the corner of drm.h that pvr_drm.h needs, usable from the kext and from user space
#include <stdint.h>

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef int64_t __s64;

#define DRM_IOCTL_BASE		'd'
#define DRM_COMMAND_BASE	0x40
// the ioctl number is only a selector for the user client, no direction or size bits
#define DRM_IOW(nr, type)	(nr)
#define DRM_IOR(nr, type)	(nr)
#define DRM_IOWR(nr, type)	(nr)

#endif
