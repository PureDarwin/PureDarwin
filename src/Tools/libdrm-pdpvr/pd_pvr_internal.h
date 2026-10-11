#ifndef _PD_PVR_INTERNAL_H
#define _PD_PVR_INTERNAL_H

// shared between the render/syncobj half of libdrm-pdpvr (pd_pvr_drm.c) and its kms and prime half
// (pd_kms.c)

#include <IOKit/IOKitLib.h>
#include <stdbool.h>
#include <stdint.h>

// the display node: descriptors opened on it are kms files of the same gpu service
#define PD_PVR_DRM_DISPLAY_NODE		"/dev/zero"
#define PD_DISPLAY_SERVICE_CLASS	"PDSun60iDisplay"

enum { PD_FILE_NONE, PD_FILE_RENDER, PD_FILE_DISPLAY };

io_connect_t pd_file_lookup(int fd, bool bind, int *kind, int *evfd);
int pd_call(io_connect_t conn, uint32_t sel, const uint64_t *in, uint32_t nin, uint64_t *out, uint32_t nout);
int pd_fail(int err);
int pd_kr_errno(kern_return_t kr);
bool pd_debug(void);

// prime: a buffer travels as a sparse temporary file whose first bytes carry the kernel's export token
int pd_prime_export(io_connect_t conn, uint32_t handle, uint32_t flags, int *prime_fd);
int pd_prime_import(io_connect_t conn, int prime_fd, uint32_t *handle);
// a display file closed: drop its framebuffers and pending flip
void pd_kms_forget(int fd);

#endif
