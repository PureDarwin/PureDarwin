#ifndef _PD_PVR_DRM_H
#define _PD_PVR_DRM_H

// libdrm-pdpvr: the libdrm calls mesa's powervr winsys makes, routed to the PDPowerVR user client

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// the render node handed out by drmGetDevices2: any descriptor opened on it becomes a gpu file
#define PD_PVR_DRM_NODE		"/dev/null"

// mmap and munmap that map a buffer through IOConnectMapMemory64 when fd is a gpu file
void *pd_drm_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int pd_drm_munmap(void *addr, size_t len);

// buffers created while on are mapped cacheable, cleaned by the kext around submits and waits
void pd_drm_set_cpu_cached(int on);

#ifdef __cplusplus
}
#endif

#endif
