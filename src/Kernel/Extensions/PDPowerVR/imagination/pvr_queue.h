/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_queue.h without drm_sched: a queue writes jobs to its cccb in submission order and keeps
// the ones in flight until the firmware's timeline ufo passes their seqno

#ifndef PVR_QUEUE_H
#define PVR_QUEUE_H

#include "pvr_cccb.h"
#include "pvr_device.h"

struct pvr_context;
struct pvr_queue;
struct pvr_job;

// a dma_fence stand-in: signalled by the cpu, or by a queue's timeline reaching seqno
struct pvr_fence {
	struct kref ref_count;
	volatile bool signaled;
	// set for the done fence of a job: the queue whose timeline ufo carries it
	struct pvr_queue *queue;
	u32 seqno;
	// the queue's context, held so the ufo outlives the fence
	struct pvr_context *ctx;
	// set with the signal when the job never ran to completion (context reset)
	int error;
	/* An immutable, flattened conjunction of retained leaf fences. */
	u32 dependency_count;
	struct pvr_fence **dependencies;
};

struct pvr_fence *pvr_fence_create_signaled(void);
struct pvr_fence *pvr_fence_join(struct pvr_fence *a, struct pvr_fence *b);
struct pvr_fence *pvr_fence_get(struct pvr_fence *f);
void pvr_fence_put(struct pvr_fence *f);
bool pvr_fence_is_signaled(struct pvr_fence *f);
void pvr_fence_signal(struct pvr_fence *f);
int pvr_fence_wait(struct pvr_device *pvr_dev, struct pvr_fence *f, u64 timeout_us);

struct pvr_queue {
	enum drm_pvr_job_type type;
	struct pvr_context *ctx;
	struct list_head node;
	atomic_t in_flight_job_count;

	struct {
		struct pvr_fw_object *fw_obj;
		u32 *value;
	} timeline_ufo;

	// last seqno handed out, and the jobs the firmware has not finished, oldest first
	u32 seqno;
	struct list_head pending;
	struct mutex pending_lock;
	bool killed;

	struct pvr_cccb cccb;
	struct pvr_fw_object *reg_state_obj;
	u32 ctx_offset;
	u64 callstack_addr;
};

struct pvr_queue *pvr_queue_create(struct pvr_context *ctx,
				   enum drm_pvr_job_type type,
				   struct drm_pvr_ioctl_create_context_args *args,
				   void *fw_ctx_map);
void pvr_queue_kill(struct pvr_queue *queue);
void pvr_queue_destroy(struct pvr_queue *queue);
int pvr_queue_device_init(struct pvr_device *pvr_dev);
void pvr_queue_device_fini(struct pvr_device *pvr_dev);
void pvr_queue_process_all(struct pvr_device *pvr_dev);
void pvr_queue_process_all_try(struct pvr_device *pvr_dev);
// pd: wait until every job any queue had taken when called has finished, the stand-in for the
// implicit fences a scanout flip waits on in linux
int pvr_queue_wait_submitted(struct pvr_device *pvr_dev, u64 timeout_us);
// the firmware reset a context: log why, mark it faulty and fail the jobs it still had
void pvr_context_reset_notify(struct pvr_device *pvr_dev,
			      const struct rogue_fwif_fwccb_cmd_context_reset_data *data);

// write a job (and its paired fragment job) to the cccb and kick; the done fence is armed
int pvr_queue_submit(struct pvr_job *job, struct pvr_fence **wait_fences, u32 wait_count);
u32 pvr_queue_job_cmds_size(struct pvr_job *job, u32 ufo_wait_count);

#endif /* PVR_QUEUE_H */
