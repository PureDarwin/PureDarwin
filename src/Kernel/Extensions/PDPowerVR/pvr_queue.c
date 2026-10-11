// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_queue.c with drm_sched and dma_fence taken out. submission is synchronous: wait for cccb
// room and a kccb slot, write the ufo waits, the command and the timeline update, then kick.
// the interrupt path walks active queues and signals what the timeline ufo has passed

#include "pvr_cccb.h"
#include "pvr_context.h"
#include "pvr_device.h"
#include "pvr_job.h"
#include "pvr_queue.h"
#include "pvr_vm.h"

#include "pvr_rogue_fwif_client.h"

#define MAX_DEADLINE_MS 30000

#define CTX_COMPUTE_CCCB_SIZE_LOG2 15
#define CTX_FRAG_CCCB_SIZE_LOG2 15
#define CTX_GEOM_CCCB_SIZE_LOG2 15
#define CTX_TRANSFER_CCCB_SIZE_LOG2 15

// how long a submission waits for cccb room before giving up
#define CCCB_SPACE_TIMEOUT_US 2000000

static int get_xfer_ctx_state_size(struct pvr_device *pvr_dev)
{
	u32 num_isp_store_registers;

	if (PVR_HAS_FEATURE(pvr_dev, xe_memory_hierarchy)) {
		num_isp_store_registers = 1;
	} else {
		int err;

		err = PVR_FEATURE_VALUE(pvr_dev, num_isp_ipp_pipes, &num_isp_store_registers);
		if (WARN_ON(err))
			return err;
	}

	return sizeof(struct rogue_fwif_frag_ctx_state) +
	       (num_isp_store_registers *
		sizeof(((struct rogue_fwif_frag_ctx_state *)0)->frag_reg_isp_store[0]));
}

static int get_frag_ctx_state_size(struct pvr_device *pvr_dev)
{
	u32 num_isp_store_registers;
	int err;

	if (PVR_HAS_FEATURE(pvr_dev, xe_memory_hierarchy)) {
		err = PVR_FEATURE_VALUE(pvr_dev, num_raster_pipes, &num_isp_store_registers);
		if (WARN_ON(err))
			return err;

		if (PVR_HAS_FEATURE(pvr_dev, gpu_multicore_support)) {
			u32 xpu_max_slaves;

			err = PVR_FEATURE_VALUE(pvr_dev, xpu_max_slaves, &xpu_max_slaves);
			if (WARN_ON(err))
				return err;

			num_isp_store_registers *= (1 + xpu_max_slaves);
		}
	} else {
		err = PVR_FEATURE_VALUE(pvr_dev, num_isp_ipp_pipes, &num_isp_store_registers);
		if (WARN_ON(err))
			return err;
	}

	return sizeof(struct rogue_fwif_frag_ctx_state) +
	       (num_isp_store_registers *
		sizeof(((struct rogue_fwif_frag_ctx_state *)0)->frag_reg_isp_store[0]));
}

static int get_ctx_state_size(struct pvr_device *pvr_dev, enum drm_pvr_job_type type)
{
	switch (type) {
	case DRM_PVR_JOB_TYPE_GEOMETRY:
		return sizeof(struct rogue_fwif_geom_ctx_state);
	case DRM_PVR_JOB_TYPE_FRAGMENT:
		return get_frag_ctx_state_size(pvr_dev);
	case DRM_PVR_JOB_TYPE_COMPUTE:
		return sizeof(struct rogue_fwif_compute_ctx_state);
	case DRM_PVR_JOB_TYPE_TRANSFER_FRAG:
		return get_xfer_ctx_state_size(pvr_dev);
	}

	WARN(1, "Invalid queue type");
	return -EINVAL;
}

static u32 get_ctx_offset(enum drm_pvr_job_type type)
{
	switch (type) {
	case DRM_PVR_JOB_TYPE_GEOMETRY:
		return offsetof(struct rogue_fwif_fwrendercontext, geom_context);
	case DRM_PVR_JOB_TYPE_FRAGMENT:
		return offsetof(struct rogue_fwif_fwrendercontext, frag_context);
	case DRM_PVR_JOB_TYPE_COMPUTE:
		return offsetof(struct rogue_fwif_fwcomputecontext, cdm_context);
	case DRM_PVR_JOB_TYPE_TRANSFER_FRAG:
		return offsetof(struct rogue_fwif_fwtransfercontext, tq_context);
	}

	return 0;
}

struct pvr_fence *
pvr_fence_create_signaled(void)
{
	struct pvr_fence *f = kzalloc(sizeof(*f), GFP_KERNEL);

	if (!f)
		return NULL;
	kref_init(&f->ref_count);
	f->signaled = true;
	return f;
}

static struct pvr_fence *
pvr_fence_create_for_queue(struct pvr_queue *queue, u32 seqno)
{
	struct pvr_fence *f = kzalloc(sizeof(*f), GFP_KERNEL);

	if (!f)
		return NULL;
	kref_init(&f->ref_count);
	f->queue = queue;
	f->seqno = seqno;
	f->ctx = pvr_context_get(queue->ctx);
	return f;
}

struct pvr_fence *
pvr_fence_get(struct pvr_fence *f)
{
	if (f)
		kref_get(&f->ref_count);
	return f;
}

struct pvr_fence *
pvr_fence_join(struct pvr_fence *a, struct pvr_fence *b)
{
	struct pvr_fence *f;
	u32 na, nb;

	if (!a || (pvr_fence_is_signaled(a) && !READ_ONCE(a->error)))
		return pvr_fence_get(b);
	if (!b || (pvr_fence_is_signaled(b) && !READ_ONCE(b->error)) || a == b)
		return pvr_fence_get(a);
	na = a->dependency_count ? a->dependency_count : 1;
	nb = b->dependency_count ? b->dependency_count : 1;
	if (na > 1024 || nb > 1024 - na)
		return NULL;
	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return NULL;
	f->dependencies = kcalloc(na + nb, sizeof(*f->dependencies), GFP_KERNEL);
	if (!f->dependencies) {
		kfree(f);
		return NULL;
	}
	kref_init(&f->ref_count);
	f->dependency_count = na + nb;
	for (u32 i = 0; i < na; i++)
		f->dependencies[i] = pvr_fence_get(a->dependency_count ? a->dependencies[i] : a);
	for (u32 i = 0; i < nb; i++)
		f->dependencies[na + i] = pvr_fence_get(b->dependency_count ? b->dependencies[i] : b);
	return f;
}

static void
pvr_fence_release(struct kref *ref)
{
	struct pvr_fence *f = container_of(ref, struct pvr_fence, ref_count);

	for (u32 i = 0; i < f->dependency_count; i++)
		pvr_fence_put(f->dependencies[i]);
	kfree(f->dependencies);
	pvr_context_put(f->ctx);
	kfree(f);
}

void
pvr_fence_put(struct pvr_fence *f)
{
	if (f)
		kref_put(&f->ref_count, pvr_fence_release);
}

bool
pvr_fence_is_signaled(struct pvr_fence *f)
{
	if (READ_ONCE(f->signaled))
		return true;
	if (f->dependency_count) {
		for (u32 i = 0; i < f->dependency_count; i++) {
			if (!pvr_fence_is_signaled(f->dependencies[i]))
				return false;
		}
		for (u32 i = 0; i < f->dependency_count; i++) {
			int err = READ_ONCE(f->dependencies[i]->error);
			if (err) {
				WRITE_ONCE(f->error, err);
				break;
			}
		}
		WRITE_ONCE(f->signaled, true);
		return true;
	}
	if (f->queue && (int)(READ_ONCE(*f->queue->timeline_ufo.value) - f->seqno) >= 0) {
		WRITE_ONCE(f->signaled, true);
		return true;
	}
	return false;
}

void
pvr_fence_signal(struct pvr_fence *f)
{
	WRITE_ONCE(f->signaled, true);
}

int
pvr_fence_wait(struct pvr_device *pvr_dev, struct pvr_fence *f, u64 timeout_us)
{
	u64 deadline = pd_pvr_now_us() + timeout_us;
	u32 gen = pd_pvr_wait_gen();

	while (!pvr_fence_is_signaled(f)) {
		if (pd_pvr_now_us() >= deadline)
			return -ETIMEDOUT;
		pd_pvr_wait_since(pvr_dev, gen, 10);
		gen = pd_pvr_wait_gen();
	}
	pvr_queue_process_all_try(pvr_dev);
	return 0;
}

static u32 ufo_cmds_size(u32 elem_count)
{
	/* We can pass at most ROGUE_FWIF_CCB_CMD_MAX_UFOS per UFO-related command. */
	u32 full_cmd_count = elem_count / ROGUE_FWIF_CCB_CMD_MAX_UFOS;
	u32 remaining_elems = elem_count % ROGUE_FWIF_CCB_CMD_MAX_UFOS;
	u32 size = full_cmd_count *
		   pvr_cccb_get_size_of_cmd_with_hdr(ROGUE_FWIF_CCB_CMD_MAX_UFOS *
						     sizeof(struct rogue_fwif_ufo));

	if (remaining_elems) {
		size += pvr_cccb_get_size_of_cmd_with_hdr(remaining_elems *
							  sizeof(struct rogue_fwif_ufo));
	}

	return size;
}

u32 pvr_queue_job_cmds_size(struct pvr_job *job, u32 ufo_wait_count)
{
	/* One UFO cmd for the fence signaling, one UFO cmd per native fence native,
	 * and a command for the job itself.
	 */
	return ufo_cmds_size(1) + ufo_cmds_size(ufo_wait_count) +
	       pvr_cccb_get_size_of_cmd_with_hdr(job->cmd_len);
}

static void pvr_queue_update_active_state_locked(struct pvr_queue *queue)
{
	struct pvr_device *pvr_dev = queue->ctx->pvr_dev;

	lockdep_assert_held(&pvr_dev->queues.lock);

	if (list_empty(&queue->node))
		return;

	if (!atomic_read(&queue->in_flight_job_count))
		list_move_tail(&queue->node, &pvr_dev->queues.idle);
	else
		list_move_tail(&queue->node, &pvr_dev->queues.active);
}

static void pvr_queue_update_active_state(struct pvr_queue *queue)
{
	struct pvr_device *pvr_dev = queue->ctx->pvr_dev;

	mutex_lock(&pvr_dev->queues.lock);
	pvr_queue_update_active_state_locked(queue);
	mutex_unlock(&pvr_dev->queues.lock);
}

static struct pvr_queue *
pvr_queue_for_job(struct pvr_job *job)
{
	return pvr_context_get_queue_for_job(job->ctx, job->type);
}

// the unsignalled queue fences become firmware ufo waits; anything else was waited on already
static void pvr_queue_submit_job_to_cccb(struct pvr_job *job, struct pvr_fence **waits,
					 u32 wait_count)
{
	struct pvr_queue *queue = pvr_queue_for_job(job);
	struct rogue_fwif_ufo ufos[ROGUE_FWIF_CCB_CMD_MAX_UFOS];
	struct pvr_cccb *cccb = &queue->cccb;
	u32 ufo_count = 0;

	/* We need to add the queue to the active list before updating the CCCB,
	 * otherwise we might miss the FW event informing us that something
	 * happened on this queue.
	 */
	atomic_inc(&queue->in_flight_job_count);
	pvr_queue_update_active_state(queue);

	for (u32 i = 0; i < wait_count; i++) {
		struct pvr_fence *f = waits[i];

		if (!f || !f->queue || pvr_fence_is_signaled(f))
			continue;

		pvr_fw_object_get_fw_addr(f->queue->timeline_ufo.fw_obj, &ufos[ufo_count].addr);
		ufos[ufo_count++].value = f->seqno;

		if (ufo_count == ARRAY_SIZE(ufos)) {
			pvr_cccb_write_command_with_header(cccb, ROGUE_FWIF_CCB_CMD_TYPE_FENCE_PR,
							   sizeof(ufos), ufos, 0, 0);
			ufo_count = 0;
		}
	}

	/* Partial render fence goes last. */
	if (job->type == DRM_PVR_JOB_TYPE_FRAGMENT && job->paired_job) {
		struct pvr_fence *jfence = job->paired_job->done_fence;

		if (!WARN_ON(!jfence || !jfence->queue)) {
			pvr_fw_object_get_fw_addr(jfence->queue->timeline_ufo.fw_obj,
						  &ufos[ufo_count].addr);
			ufos[ufo_count++].value = jfence->seqno;
		}
	}

	if (ufo_count) {
		pvr_cccb_write_command_with_header(cccb, ROGUE_FWIF_CCB_CMD_TYPE_FENCE_PR,
						   sizeof(ufos[0]) * ufo_count, ufos, 0, 0);
	}

	if (job->type == DRM_PVR_JOB_TYPE_GEOMETRY && job->paired_job) {
		struct rogue_fwif_cmd_geom *cmd = job->cmd;

		/* Reference value for the partial render test is the current queue fence
		 * seqno minus one.
		 */
		pvr_fw_object_get_fw_addr(queue->timeline_ufo.fw_obj,
					  &cmd->partial_render_geom_frag_fence.addr);
		cmd->partial_render_geom_frag_fence.value = job->done_fence->seqno - 1;
	}

	/* Submit job to FW */
	pvr_cccb_write_command_with_header(cccb, job->fw_ccb_cmd_type, job->cmd_len, job->cmd,
					   job->id, job->id);

	/* Signal the job fence. */
	pvr_fw_object_get_fw_addr(queue->timeline_ufo.fw_obj, &ufos[0].addr);
	ufos[0].value = job->done_fence->seqno;
	pvr_cccb_write_command_with_header(cccb, ROGUE_FWIF_CCB_CMD_TYPE_UPDATE,
					   sizeof(ufos[0]), ufos, 0, 0);

	mutex_lock(&queue->pending_lock);
	list_add_tail(&job->queue_node, &queue->pending);
	pvr_job_get(job);
	mutex_unlock(&queue->pending_lock);
}

static u32 count_native_waits(struct pvr_fence **waits, u32 wait_count)
{
	u32 n = 0;

	for (u32 i = 0; i < wait_count; i++) {
		if (waits[i] && waits[i]->queue && !pvr_fence_is_signaled(waits[i]))
			n++;
	}
	return n;
}

static int wait_for_cccb_space(struct pvr_device *pvr_dev, struct pvr_queue *queue, u32 size)
{
	u64 deadline = pd_pvr_now_us() + CCCB_SPACE_TIMEOUT_US;
	u32 gen = pd_pvr_wait_gen();

	while (!pvr_cccb_cmdseq_fits(&queue->cccb, size)) {
		if (pd_pvr_now_us() >= deadline)
			return -EBUSY;
		pd_pvr_wait_since(pvr_dev, gen, 10);
		gen = pd_pvr_wait_gen();
	}
	return 0;
}

int pvr_queue_submit(struct pvr_job *job, struct pvr_fence **waits, u32 wait_count)
{
	struct pvr_device *pvr_dev = job->pvr_dev;
	struct pvr_queue *queue = pvr_queue_for_job(job);
	struct pvr_job *frag_job = job->paired_job;
	struct pvr_queue *frag_queue = frag_job ? pvr_queue_for_job(frag_job) : NULL;
	int err;

	if (!queue || queue->killed || (frag_job && (!frag_queue || frag_queue->killed)))
		return -EINVAL;
	if (atomic_read(&job->ctx->faulty))
		return -EIO;

	/* The only kind of jobs that can be paired are geometry and fragment, and
	 * paired jobs must target the same context and point to the same HWRT.
	 */
	if (WARN_ON(frag_job &&
		    (job->type != DRM_PVR_JOB_TYPE_GEOMETRY ||
		     frag_job->type != DRM_PVR_JOB_TYPE_FRAGMENT ||
		     job->hwrt != frag_job->hwrt ||
		     job->ctx != frag_job->ctx)))
		return -EINVAL;

	err = wait_for_cccb_space(pvr_dev, queue,
				  pvr_queue_job_cmds_size(job, count_native_waits(waits, wait_count)));
	if (!err && frag_job)
		err = wait_for_cccb_space(pvr_dev, frag_queue, pvr_queue_job_cmds_size(frag_job, 1));
	if (err)
		return err;

	err = pvr_kccb_reserve_slot_sync(pvr_dev);
	if (err)
		return err;

	job->done_fence = pvr_fence_create_for_queue(queue, ++queue->seqno);
	if (frag_job)
		frag_job->done_fence = pvr_fence_create_for_queue(frag_queue, ++frag_queue->seqno);
	if (!job->done_fence || (frag_job && !frag_job->done_fence)) {
		// give the reserved kccb slot back
		pd_pvr_lock(pvr_dev->kccb.lock);
		pvr_dev->kccb.reserved_count--;
		pd_pvr_unlock(pvr_dev->kccb.lock);
		return -ENOMEM;
	}

	/* Submit our job to the CCCB */
	pvr_queue_submit_job_to_cccb(job, waits, wait_count);

	if (frag_job) {
		/* Submit the fragment job along the geometry job and send a combined kick. */
		pvr_queue_submit_job_to_cccb(frag_job, NULL, 0);
		pvr_cccb_send_kccb_combined_kick(pvr_dev,
						 &queue->cccb, &frag_queue->cccb,
						 pvr_context_get_fw_addr(job->ctx) +
						 queue->ctx_offset,
						 pvr_context_get_fw_addr(frag_job->ctx) +
						 frag_queue->ctx_offset,
						 job->hwrt,
						 frag_job->fw_ccb_cmd_type ==
						 ROGUE_FWIF_CCB_CMD_TYPE_FRAG_PR);
	} else {
		pvr_cccb_send_kccb_kick(pvr_dev, &queue->cccb,
					pvr_context_get_fw_addr(job->ctx) + queue->ctx_offset,
					job->hwrt);
	}

	return 0;
}

// moves the jobs the timeline has passed onto done; the caller drops them after the queues lock,
// a last job put can release its context, whose teardown takes that lock
static void
pvr_queue_signal_done_fences(struct pvr_queue *queue, struct list_head *done)
{
	struct pvr_job *job, *tmp_job;
	u32 cur_seqno;

	mutex_lock(&queue->pending_lock);
	cur_seqno = READ_ONCE(*queue->timeline_ufo.value);
	list_for_each_entry_safe(job, tmp_job, &queue->pending, queue_node) {
		if ((int)(cur_seqno - job->done_fence->seqno) < 0)
			break;

		pvr_fence_signal(job->done_fence);
		atomic_dec(&queue->in_flight_job_count);
		list_move_tail(&job->queue_node, done);
	}
	mutex_unlock(&queue->pending_lock);
}

static void
pvr_queue_put_done(struct list_head *done)
{
	struct pvr_job *job, *tmp_job;

	list_for_each_entry_safe(job, tmp_job, done, queue_node) {
		list_del(&job->queue_node);
		pvr_job_put(job);
	}
}

static void pvr_queue_process(struct pvr_queue *queue, struct list_head *done)
{
	lockdep_assert_held(&queue->ctx->pvr_dev->queues.lock);

	pvr_queue_signal_done_fences(queue, done);
	pvr_queue_update_active_state_locked(queue);
}

u32 pd_diag_process_started, pd_diag_process_finished;

void pvr_queue_process_all(struct pvr_device *pvr_dev)
{
	struct pvr_queue *queue, *tmp;
	LIST_HEAD(done);
	u32 runs;

	mutex_lock(&pvr_dev->queues.lock);
	__atomic_add_fetch(&pd_diag_process_started, 1, __ATOMIC_RELAXED);
	list_for_each_entry_safe(queue, tmp, &pvr_dev->queues.active, node)
		pvr_queue_process(queue, &done);
	mutex_unlock(&pvr_dev->queues.lock);
	pvr_queue_put_done(&done);
	runs = __atomic_add_fetch(&pd_diag_process_finished, 1, __ATOMIC_RELAXED);
	// pdpvrslots: interrupt-driven processing still running, at every power of two from 1024
	if (pd_pvr_slot_diag() && runs >= 1024 && (runs & (runs - 1)) == 0)
		pd_pvr_log("queue processing runs %u, irqs %u, jobs created %u released %u\n", runs,
			   pvr_dev->irq_count, pd_diag_jobs_created, pd_diag_jobs_released);
}

// completed jobs are released from the gpu interrupt; waiters and submitters also retire them,
// so a missed or stuck interrupt cannot pin every job and its hwrt dataset
void pvr_queue_process_all_try(struct pvr_device *pvr_dev)
{
	struct pvr_queue *queue, *tmp;
	LIST_HEAD(done);

	if (!mutex_trylock(&pvr_dev->queues.lock))
		return;
	list_for_each_entry_safe(queue, tmp, &pvr_dev->queues.active, node)
		pvr_queue_process(queue, &done);
	mutex_unlock(&pvr_dev->queues.lock);
	pvr_queue_put_done(&done);
}

#define PVR_QUEUE_WAIT_MAX	64

int pvr_queue_wait_submitted(struct pvr_device *pvr_dev, u64 timeout_us)
{
	struct pvr_fence *fences[PVR_QUEUE_WAIT_MAX];
	struct list_head *lists[2] = { &pvr_dev->queues.active, &pvr_dev->queues.idle };
	struct pvr_queue *queue;
	u32 n = 0;
	int err = 0;

	mutex_lock(&pvr_dev->queues.lock);
	for (int l = 0; l < 2; l++) {
		list_for_each_entry(queue, lists[l], node) {
			if (n == PVR_QUEUE_WAIT_MAX)
				break;
			if ((int)(READ_ONCE(*queue->timeline_ufo.value) - READ_ONCE(queue->seqno)) >= 0)
				continue;
			fences[n] = pvr_fence_create_for_queue(queue, READ_ONCE(queue->seqno));
			if (fences[n])
				n++;
		}
	}
	mutex_unlock(&pvr_dev->queues.lock);

	for (u32 i = 0; i < n; i++) {
		if (!err)
			err = pvr_fence_wait(pvr_dev, fences[i], timeout_us);
		pvr_fence_put(fences[i]);
	}
	return err;
}

static u32 get_dm_type(struct pvr_queue *queue)
{
	switch (queue->type) {
	case DRM_PVR_JOB_TYPE_GEOMETRY:
		return PVR_FWIF_DM_GEOM;
	case DRM_PVR_JOB_TYPE_TRANSFER_FRAG:
	case DRM_PVR_JOB_TYPE_FRAGMENT:
		return PVR_FWIF_DM_FRAG;
	case DRM_PVR_JOB_TYPE_COMPUTE:
		return PVR_FWIF_DM_CDM;
	}

	return ~0;
}

static void init_fw_context(struct pvr_queue *queue, void *fw_ctx_map)
{
	struct pvr_context *ctx = queue->ctx;
	struct pvr_fw_object *fw_mem_ctx_obj = pvr_vm_get_fw_mem_context(ctx->vm_ctx);
	struct rogue_fwif_fwcommoncontext *cctx_fw;
	struct pvr_cccb *cccb = &queue->cccb;

	cctx_fw = (struct rogue_fwif_fwcommoncontext *)((u8 *)fw_ctx_map + queue->ctx_offset);
	cctx_fw->ccbctl_fw_addr = cccb->ctrl_fw_addr;
	cctx_fw->ccb_fw_addr = cccb->cccb_fw_addr;

	cctx_fw->dm = get_dm_type(queue);
	cctx_fw->priority = ctx->priority;
	cctx_fw->priority_seq_num = 0;
	cctx_fw->max_deadline_ms = MAX_DEADLINE_MS;
	cctx_fw->pid = pd_current_pid();
	cctx_fw->server_common_context_id = ctx->ctx_id;

	pvr_fw_object_get_fw_addr(fw_mem_ctx_obj, &cctx_fw->fw_mem_context_fw_addr);

	pvr_fw_object_get_fw_addr(queue->reg_state_obj, &cctx_fw->context_state_addr);
}

static int pvr_queue_cleanup_fw_context(struct pvr_queue *queue)
{
	if (!queue->ctx->fw_obj)
		return 0;

	return pvr_fw_structure_cleanup(queue->ctx->pvr_dev,
					ROGUE_FWIF_CLEANUP_FWCOMMONCONTEXT,
					queue->ctx->fw_obj, queue->ctx_offset);
}

static void reg_state_init(void *cpu_ptr, void *priv)
{
	struct pvr_queue *queue = priv;

	if (queue->type == DRM_PVR_JOB_TYPE_GEOMETRY) {
		struct rogue_fwif_geom_ctx_state *geom_ctx_state_fw = cpu_ptr;

		geom_ctx_state_fw->geom_core[0].geom_reg_vdm_call_stack_pointer_init =
			queue->callstack_addr;
	}
}

struct pvr_queue *pvr_queue_create(struct pvr_context *ctx,
				   enum drm_pvr_job_type type,
				   struct drm_pvr_ioctl_create_context_args *args,
				   void *fw_ctx_map)
{
	static const struct {
		u32 cccb_size;
		const char *name;
	} props[] = {
		[DRM_PVR_JOB_TYPE_GEOMETRY] = {
			.cccb_size = CTX_GEOM_CCCB_SIZE_LOG2,
			.name = "geometry",
		},
		[DRM_PVR_JOB_TYPE_FRAGMENT] = {
			.cccb_size = CTX_FRAG_CCCB_SIZE_LOG2,
			.name = "fragment"
		},
		[DRM_PVR_JOB_TYPE_COMPUTE] = {
			.cccb_size = CTX_COMPUTE_CCCB_SIZE_LOG2,
			.name = "compute"
		},
		[DRM_PVR_JOB_TYPE_TRANSFER_FRAG] = {
			.cccb_size = CTX_TRANSFER_CCCB_SIZE_LOG2,
			.name = "transfer_frag"
		},
	};
	struct pvr_device *pvr_dev = ctx->pvr_dev;
	struct pvr_queue *queue;
	int ctx_state_size, err;
	void *cpu_map;

	if (WARN_ON(type >= ARRAY_SIZE(props)))
		return ERR_PTR(-EINVAL);

	switch (ctx->type) {
	case DRM_PVR_CTX_TYPE_RENDER:
		if (type != DRM_PVR_JOB_TYPE_GEOMETRY &&
		    type != DRM_PVR_JOB_TYPE_FRAGMENT)
			return ERR_PTR(-EINVAL);
		break;
	case DRM_PVR_CTX_TYPE_COMPUTE:
		if (type != DRM_PVR_JOB_TYPE_COMPUTE)
			return ERR_PTR(-EINVAL);
		break;
	case DRM_PVR_CTX_TYPE_TRANSFER_FRAG:
		if (type != DRM_PVR_JOB_TYPE_TRANSFER_FRAG)
			return ERR_PTR(-EINVAL);
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	ctx_state_size = get_ctx_state_size(pvr_dev, type);
	if (ctx_state_size < 0)
		return ERR_PTR(ctx_state_size);

	queue = kzalloc(sizeof(*queue), GFP_KERNEL);
	if (!queue)
		return ERR_PTR(-ENOMEM);

	queue->type = type;
	queue->ctx_offset = get_ctx_offset(type);
	queue->ctx = ctx;
	queue->callstack_addr = args->callstack_addr;
	INIT_LIST_HEAD(&queue->node);
	INIT_LIST_HEAD(&queue->pending);
	mutex_init(&queue->pending_lock);

	err = pvr_cccb_init(pvr_dev, &queue->cccb, props[type].cccb_size, props[type].name);
	if (err)
		goto err_free_queue;

	err = pvr_fw_object_create(pvr_dev, ctx_state_size,
				   PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
				   reg_state_init, queue, &queue->reg_state_obj);
	if (err)
		goto err_cccb_fini;

	init_fw_context(queue, fw_ctx_map);

	if (type != DRM_PVR_JOB_TYPE_GEOMETRY && type != DRM_PVR_JOB_TYPE_FRAGMENT &&
	    args->callstack_addr) {
		err = -EINVAL;
		goto err_release_reg_state;
	}

	cpu_map = pvr_fw_object_create_and_map(pvr_dev, sizeof(*queue->timeline_ufo.value),
					       PVR_BO_FW_FLAGS_DEVICE_UNCACHED,
					       NULL, NULL, &queue->timeline_ufo.fw_obj);
	if (!cpu_map || IS_ERR(cpu_map)) {
		err = -ENOMEM;
		goto err_release_reg_state;
	}

	queue->timeline_ufo.value = cpu_map;

	mutex_lock(&pvr_dev->queues.lock);
	list_add_tail(&queue->node, &pvr_dev->queues.idle);
	mutex_unlock(&pvr_dev->queues.lock);

	return queue;

err_release_reg_state:
	pvr_fw_object_destroy(queue->reg_state_obj);

err_cccb_fini:
	pvr_cccb_fini(&queue->cccb);

err_free_queue:
	mutex_destroy(&queue->pending_lock);
	kfree(queue);

	return ERR_PTR(err);
}

void pvr_queue_kill(struct pvr_queue *queue)
{
	if (queue)
		queue->killed = true;
}

void pvr_queue_destroy(struct pvr_queue *queue)
{
	if (!queue)
		return;

	mutex_lock(&queue->ctx->pvr_dev->queues.lock);
	list_del_init(&queue->node);
	mutex_unlock(&queue->ctx->pvr_dev->queues.lock);

	WARN_ON(!list_empty(&queue->pending));

	pvr_queue_cleanup_fw_context(queue);

	pvr_fw_object_unmap_and_destroy(queue->timeline_ufo.fw_obj);
	pvr_fw_object_destroy(queue->reg_state_obj);
	pvr_cccb_fini(&queue->cccb);
	mutex_destroy(&queue->pending_lock);
	kfree(queue);
}

int pvr_queue_device_init(struct pvr_device *pvr_dev)
{
	INIT_LIST_HEAD(&pvr_dev->queues.active);
	INIT_LIST_HEAD(&pvr_dev->queues.idle);
	mutex_init(&pvr_dev->queues.lock);

	return 0;
}

void pvr_queue_device_fini(struct pvr_device *pvr_dev)
{
	mutex_destroy(&pvr_dev->queues.lock);
}

static const char *
reset_reason_name(u32 reason)
{
	switch (reason) {
	case ROGUE_CONTEXT_RESET_REASON_NONE: return "none";
	case ROGUE_CONTEXT_RESET_REASON_GUILTY_LOCKUP: return "guilty lockup";
	case ROGUE_CONTEXT_RESET_REASON_INNOCENT_LOCKUP: return "innocent lockup";
	case ROGUE_CONTEXT_RESET_REASON_GUILTY_OVERRUNING: return "guilty overrun";
	case ROGUE_CONTEXT_RESET_REASON_INNOCENT_OVERRUNING: return "innocent overrun";
	case ROGUE_CONTEXT_RESET_REASON_HARD_CONTEXT_SWITCH: return "hard context switch";
	case ROGUE_CONTEXT_RESET_REASON_FW_WATCHDOG: return "fw watchdog";
	case ROGUE_CONTEXT_RESET_REASON_FW_PAGEFAULT: return "fw page fault";
	case ROGUE_CONTEXT_RESET_REASON_FW_EXEC_ERR: return "fw exec error";
	case ROGUE_CONTEXT_RESET_REASON_HOST_WDG_FW_ERR: return "host watchdog";
	case ROGUE_CONTEXT_GEOM_OOM_DISABLED: return "geometry oom disabled";
	}
	return "unknown";
}

static const char *
hwr_type_name(u32 type)
{
	static const char *const names[] = {
		"unknown", "overrun", "poll failure", "bif0 fault", "bif1 fault", "texas bif0 fault",
		"mmu fault", "mmu meta fault", "mips tlb fault", "ecc fault", "mmu riscv fault",
	};

	return type < ARRAY_SIZE(names) ? names[type] : "?";
}

// the firmware's last hardware recovery record, and the bif fault registers as they stand
static void
log_hwr_info(struct pvr_device *pvr_dev)
{
	struct rogue_fwif_hwrinfobuf *buf = pvr_dev->fw_dev.hwrinfobuf;
	u32 idx = (READ_ONCE(buf->write_index) + ROGUE_FWIF_HWINFO_MAX - 1) % ROGUE_FWIF_HWINFO_MAX;
	struct rogue_hwrinfo *hi = &buf->hwr_info[idx];

	pd_pvr_log("hwr #%u (counter %u): %s, dm %u, event status 0x%x, flags 0x%x, active hwrt 0x%x\n",
		   hi->hwr_number, buf->hwr_counter, hwr_type_name(hi->hwr_type), hi->dm, hi->event_status,
		   hi->hwr_recovery_flags, hi->active_hwrt_data);
	switch (hi->hwr_type) {
	case ROGUE_HWRTYPE_BIF0FAULT:
	case ROGUE_HWRTYPE_BIF1FAULT:
	case ROGUE_HWRTYPE_TEXASBIF0FAULT:
		pd_pvr_log("hwr bif fault: req status 0x%llx mmu status 0x%llx pc 0x%llx\n",
			   (u64)hi->hwr_data.bif_info.bif_req_status,
			   (u64)hi->hwr_data.bif_info.bif_mmu_status,
			   (u64)hi->hwr_data.bif_info.pc_address);
		break;
	case ROGUE_HWRTYPE_MMUFAULT:
	case ROGUE_HWRTYPE_MMUMETAFAULT:
		pd_pvr_log("hwr mmu fault: status 0x%llx 0x%llx pc 0x%llx\n",
			   (u64)hi->hwr_data.mmu_info.mmu_status[0], (u64)hi->hwr_data.mmu_info.mmu_status[1],
			   (u64)hi->hwr_data.mmu_info.pc_address);
		break;
	case ROGUE_HWRTYPE_MIPSTLBFAULT:
		pd_pvr_log("hwr mips tlb fault: bad addr 0x%x entry lo 0x%x\n",
			   hi->hwr_data.tlb_info.bad_addr, hi->hwr_data.tlb_info.entry_lo);
		break;
	case ROGUE_HWRTYPE_POLLFAILURE:
		pd_pvr_log("hwr poll failure: thread %u cr 0x%x mask 0x%x last 0x%x\n",
			   hi->hwr_data.poll_info.thread_num, hi->hwr_data.poll_info.cr_poll_addr,
			   hi->hwr_data.poll_info.cr_poll_mask, hi->hwr_data.poll_info.cr_poll_last_value);
		break;
	default:
		break;
	}
	pd_pvr_log("bif fault bank0 mmu 0x%llx req 0x%llx, texas bank0 mmu 0x%llx req 0x%llx\n",
		   pvr_cr_read64(pvr_dev, ROGUE_CR_BIF_FAULT_BANK0_MMU_STATUS),
		   pvr_cr_read64(pvr_dev, ROGUE_CR_BIF_FAULT_BANK0_REQ_STATUS),
		   pvr_cr_read64(pvr_dev, ROGUE_CR_TEXAS_BIF_FAULT_BANK0_MMU_STATUS),
		   pvr_cr_read64(pvr_dev, ROGUE_CR_TEXAS_BIF_FAULT_BANK0_REQ_STATUS));
}

// every job still pending on the queue completes now, with an error on its fence
static u32
pvr_queue_fail_pending(struct pvr_queue *queue, int error)
{
	struct pvr_job *job, *tmp_job;
	LIST_HEAD(done);
	u32 n = 0;

	if (!queue)
		return 0;
	mutex_lock(&queue->pending_lock);
	list_for_each_entry_safe(job, tmp_job, &queue->pending, queue_node) {
		job->done_fence->error = error;
		pvr_fence_signal(job->done_fence);
		atomic_dec(&queue->in_flight_job_count);
		list_move_tail(&job->queue_node, &done);
		n++;
	}
	mutex_unlock(&queue->pending_lock);

	list_for_each_entry_safe(job, tmp_job, &done, queue_node) {
		list_del(&job->queue_node);
		pvr_job_put(job);
	}
	pvr_queue_update_active_state(queue);
	return n;
}

void
pvr_context_reset_notify(struct pvr_device *pvr_dev,
			 const struct rogue_fwif_fwccb_cmd_context_reset_data *data)
{
	struct pvr_context *ctx = pvr_context_lookup_id(pvr_dev, data->server_common_context_id);
	u32 failed = 0;

	pd_pvr_log("context %u reset: %s (%u), dm %u, job ref %u, flags 0x%x%s%s, pc 0x%llx, fault address 0x%llx\n",
		   data->server_common_context_id, reset_reason_name(data->reset_reason), data->reset_reason,
		   data->dm, data->reset_job_ref, data->flags,
		   (data->flags & ROGUE_FWIF_FWCCB_CMD_CONTEXT_RESET_FLAG_PF) ? " page-fault" : "",
		   (data->flags & ROGUE_FWIF_FWCCB_CMD_CONTEXT_RESET_FLAG_ALL_CTXS) ? " all-contexts" : "",
		   (u64)data->pc_address, (u64)data->fault_address);
	log_hwr_info(pvr_dev);

	if (!ctx) {
		pd_pvr_log("context %u is gone, nothing to fail\n", data->server_common_context_id);
		return;
	}
	// further submits get -EIO; what was queued completes now with an error
	atomic_set(&ctx->faulty, 1);
	switch (ctx->type) {
	case DRM_PVR_CTX_TYPE_RENDER:
		failed += pvr_queue_fail_pending(ctx->queues.geometry, -EIO);
		failed += pvr_queue_fail_pending(ctx->queues.fragment, -EIO);
		break;
	case DRM_PVR_CTX_TYPE_COMPUTE:
		failed += pvr_queue_fail_pending(ctx->queues.compute, -EIO);
		break;
	case DRM_PVR_CTX_TYPE_TRANSFER_FRAG:
		failed += pvr_queue_fail_pending(ctx->queues.transfer, -EIO);
		break;
	}
	pd_pvr_log("context %u marked faulty, %u pending jobs failed\n", data->server_common_context_id, failed);
	pvr_context_put(ctx);
}
