/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// what the ported imagination files expect from pvr_device.h, pvr_fw.h, pvr_gem.h, pvr_ccb.h and
// pvr_power.h, on top of the device and firmware state in pvr_port.h

#ifndef _PDPOWERVR_PVR_DEVICE_H
#define _PDPOWERVR_PVR_DEVICE_H

#include <linux/pd_kernel.h>

#include "pvr_port.h"
#include "include/pvr_drm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define from_pvr_device(pvr_dev) (pvr_dev)

// pvr_device.h's per-client state; the user client owns one
struct pvr_file {
	struct pvr_device *pvr_dev;
	struct xarray ctx_handles;
	struct xarray free_list_handles;
	struct xarray hwrt_handles;
	struct xarray vm_ctx_handles;
	struct list_head contexts;
	// pd: buffer object and sync object handles, the drm file's tables
	struct xarray bo_handles;
	struct xarray syncobj_handles;
};

#define from_pvr_file(pvr_file) (pvr_file)

// the ioctl layer in pvr_drv.c, for the user client
int pvr_ioctl(struct pvr_device *pvr_dev, struct pvr_file *pvr_file, u32 nr, void *args, u32 size);
u32 pvr_ioctl_args_size(u32 nr);
struct pvr_file *pvr_file_open(struct pvr_device *pvr_dev);
void pvr_file_close(struct pvr_file *pvr_file);
void pvr_device_jobs_init(struct pvr_device *pvr_dev);
int pvr_selftest_null_compute(struct pvr_device *pvr_dev);
#define to_pvr_file(file) (file)
#define PVR_BO_FW_FLAGS_DEVICE_CACHED (0)

bool pvr_device_has_uapi_quirk(struct pvr_device *pvr_dev, u32 quirk);
bool pvr_device_has_uapi_enhancement(struct pvr_device *pvr_dev, u32 enhancement);
bool pvr_device_has_feature(struct pvr_device *pvr_dev, u32 feature);
#define to_pvr_device(x) (x)
#define drm_dev_enter(dev, idx) (*(idx) = 0, true)
#define drm_dev_exit(idx) do { } while (0)

#ifdef __cplusplus
}
#endif

#endif
