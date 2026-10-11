/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// binary sync objects, standing in for drm_syncobj: each holds the fence last put in it

#ifndef PVR_SYNC_H
#define PVR_SYNC_H

#include <linux/types.h>

struct pvr_file;
struct pvr_fence;
struct pvr_device;

#define PVR_SYNCOBJ_CREATE_SIGNALED	(1u << 0)
#define PVR_SYNCOBJ_WAIT_ALL		(1u << 0)

int pvr_syncobj_create(struct pvr_file *pvr_file, u32 flags, u32 *handle_out);
int pvr_syncobj_destroy(struct pvr_file *pvr_file, u32 handle);
struct pvr_fence *pvr_syncobj_fence_get(struct pvr_file *pvr_file, u32 handle);
int pvr_syncobj_replace_fence(struct pvr_file *pvr_file, u32 handle, struct pvr_fence *fence);
int pvr_syncobj_reset(struct pvr_file *pvr_file, u32 handle);
int pvr_syncobj_signal(struct pvr_file *pvr_file, u32 handle);
int pvr_syncobj_wait(struct pvr_device *pvr_dev, struct pvr_file *pvr_file, const u32 *handles,
		     u32 count, u32 flags, u64 timeout_us, u32 *first_signaled);
void pvr_destroy_syncobjs_for_file(struct pvr_file *pvr_file);

int pvr_syncobj_transfer(struct pvr_file *pvr_file, u32 src, u32 dst, bool join);

#endif /* PVR_SYNC_H */
