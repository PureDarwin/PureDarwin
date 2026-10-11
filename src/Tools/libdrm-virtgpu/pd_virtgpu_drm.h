#ifndef _PD_VIRTGPU_DRM_H
#define _PD_VIRTGPU_DRM_H

// libdrm-virtgpu: the libdrm calls mesa's venus virtgpu backend makes, routed to IOVirtIOGPU's venus client

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// the render node handed out by drmGetDevices2: any descriptor opened on it becomes a gpu file
#define PD_VIRTGPU_DRM_NODE	"/dev/null"

// mmap and munmap that map a blob through IOConnectMapMemory64 when fd is a gpu file
void *pd_drm_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int pd_drm_munmap(void *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
