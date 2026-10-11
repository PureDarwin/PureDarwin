/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (c) 2023 Imagination Technologies Ltd. */

#ifndef PVR_JOB_H
#define PVR_JOB_H

#include "include/pvr_drm.h"

#include <linux/kref.h>
#include <linux/types.h>

/* Forward declaration from "pvr_context.h". */
struct pvr_context;

/* Forward declarations from "pvr_device.h". */
struct pvr_device;
struct pvr_file;

/* Forward declarations from "pvr_hwrt.h". */
struct pvr_hwrt_data;

/* Forward declaration from "pvr_queue.h". */
struct pvr_queue;
struct pvr_fence;

struct pvr_job {
	/** @ref_count: Refcount for job. */
	struct kref ref_count;

	/** @type: Type of job. */
	enum drm_pvr_job_type type;

	/** @id: Job ID number. */
	u32 id;

	/**
	 * @paired_job: Job paired to this job.
	 *
	 * This field is only meaningful for geometry and fragment jobs.
	 */
	struct pvr_job *paired_job;

	/** @done_fence: Fence to signal when the job is done. */
	struct pvr_fence *done_fence;

	/** @pvr_dev: Device pointer. */
	struct pvr_device *pvr_dev;

	/** @ctx: Pointer to owning context. */
	struct pvr_context *ctx;

	/** @cmd: Command data. Format depends on @type. */
	void *cmd;

	/** @cmd_len: Length of command data, in bytes. */
	u32 cmd_len;

	/**
	 * @fw_ccb_cmd_type: Firmware CCB command type. Must be one of %ROGUE_FWIF_CCB_CMD_TYPE_*.
	 */
	u32 fw_ccb_cmd_type;

	/** @hwrt: HWRT object. Will be NULL for compute and transfer jobs. */
	struct pvr_hwrt_data *hwrt;

	// pd: on its queue's pending list until the timeline passes it
	struct list_head queue_node;
};

static __always_inline struct pvr_job *
pvr_job_get(struct pvr_job *job)
{
	if (job)
		kref_get(&job->ref_count);

	return job;
}

void pvr_job_put(struct pvr_job *job);

int pvr_submit_jobs(struct pvr_device *pvr_dev, struct pvr_file *pvr_file,
		    struct drm_pvr_ioctl_submit_jobs_args *args);

#endif /* PVR_JOB_H */
