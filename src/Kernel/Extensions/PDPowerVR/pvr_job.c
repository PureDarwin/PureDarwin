// SPDX-License-Identifier: GPL-2.0-only OR MIT
/* Copyright (c) 2023 Imagination Technologies Ltd. */

// pvr_job.c: command stream processing as in linux. the drm_sched and dma_resv parts are replaced
// by a synchronous submit: syncobj waits become ufo waits, signals take the job's done fence

#include "pvr_context.h"
#include "pvr_device.h"
#include "pvr_drv.h"
#include "pvr_gem.h"
#include "pvr_hwrt.h"
#include "pvr_job.h"
#include "pvr_mmu.h"
#include "pvr_power.h"
#include "pvr_queue.h"
#include "pvr_rogue_fwif.h"
#include "pvr_rogue_fwif_client.h"
#include "pvr_stream.h"
#include "pvr_stream_defs.h"
#include "pvr_sync.h"

#include <linux/types.h>
#include "include/pvr_drm.h"

u32 pd_diag_jobs_created, pd_diag_jobs_released;

static void pvr_job_release(struct kref *kref)
{
	struct pvr_job *job = container_of(kref, struct pvr_job, ref_count);

	__atomic_add_fetch(&pd_diag_jobs_released, 1, __ATOMIC_RELAXED);
	xa_erase(&job->pvr_dev->job_ids, job->id);

	pvr_hwrt_data_put(job->hwrt);
	pvr_context_put(job->ctx);

	pvr_fence_put(job->done_fence);

	kfree(job->cmd);
	kfree(job);
}

/**
 * pvr_job_put() - Release reference on job
 * @job: Target job.
 */
void
pvr_job_put(struct pvr_job *job)
{
	if (job)
		kref_put(&job->ref_count, pvr_job_release);
}

/**
 * pvr_job_process_stream() - Build job FW structure from stream
 * @pvr_dev: Device pointer.
 * @cmd_defs: Stream definition.
 * @stream: Pointer to command stream.
 * @stream_size: Size of command stream, in bytes.
 * @job: Pointer to job.
 *
 * Caller is responsible for freeing the output structure.
 *
 * Returns:
 *  * 0 on success,
 *  * -%ENOMEM on out of memory, or
 *  * -%EINVAL on malformed stream.
 */
static int
pvr_job_process_stream(struct pvr_device *pvr_dev, const struct pvr_stream_cmd_defs *cmd_defs,
		       void *stream, u32 stream_size, struct pvr_job *job)
{
	int err;

	job->cmd = kzalloc(cmd_defs->dest_size, GFP_KERNEL);
	if (!job->cmd)
		return -ENOMEM;

	job->cmd_len = cmd_defs->dest_size;

	err = pvr_stream_process(pvr_dev, cmd_defs, stream, stream_size, job->cmd);
	if (err) {
		// pd: pvr_job_release() frees job->cmd too, so drop it here once
		kfree(job->cmd);
		job->cmd = NULL;
		job->cmd_len = 0;
	}

	return err;
}

static int pvr_fw_cmd_init(struct pvr_device *pvr_dev, struct pvr_job *job,
			   const struct pvr_stream_cmd_defs *stream_def,
			   u64 stream_userptr, u32 stream_len)
{
	void *stream;
	int err;

	stream = memdup_user(u64_to_user_ptr(stream_userptr), stream_len);
	if (IS_ERR(stream))
		return PTR_ERR(stream);

	err = pvr_job_process_stream(pvr_dev, stream_def, stream, stream_len, job);

	kfree(stream);
	return err;
}

static u32
convert_geom_flags(u32 in_flags)
{
	u32 out_flags = 0;

	if (in_flags & DRM_PVR_SUBMIT_JOB_GEOM_CMD_FIRST)
		out_flags |= ROGUE_GEOM_FLAGS_FIRSTKICK;
	if (in_flags & DRM_PVR_SUBMIT_JOB_GEOM_CMD_LAST)
		out_flags |= ROGUE_GEOM_FLAGS_LASTKICK;
	if (in_flags & DRM_PVR_SUBMIT_JOB_GEOM_CMD_SINGLE_CORE)
		out_flags |= ROGUE_GEOM_FLAGS_SINGLE_CORE;

	return out_flags;
}

static u32
convert_frag_flags(u32 in_flags)
{
	u32 out_flags = 0;

	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_SINGLE_CORE)
		out_flags |= ROGUE_FRAG_FLAGS_SINGLE_CORE;
	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_DEPTHBUFFER)
		out_flags |= ROGUE_FRAG_FLAGS_DEPTHBUFFER;
	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_STENCILBUFFER)
		out_flags |= ROGUE_FRAG_FLAGS_STENCILBUFFER;
	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_PREVENT_CDM_OVERLAP)
		out_flags |= ROGUE_FRAG_FLAGS_PREVENT_CDM_OVERLAP;
	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_SCRATCHBUFFER)
		out_flags |= ROGUE_FRAG_FLAGS_SCRATCHBUFFER;
	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_GET_VIS_RESULTS)
		out_flags |= ROGUE_FRAG_FLAGS_GET_VIS_RESULTS;
	if (in_flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_DISABLE_PIXELMERGE)
		out_flags |= ROGUE_FRAG_FLAGS_DISABLE_PIXELMERGE;

	return out_flags;
}

static int
pvr_geom_job_fw_cmd_init(struct pvr_job *job,
			 struct drm_pvr_job *args)
{
	struct rogue_fwif_cmd_geom *cmd;
	int err;

	if (args->flags & ~DRM_PVR_SUBMIT_JOB_GEOM_CMD_FLAGS_MASK)
		return -EINVAL;

	if (job->ctx->type != DRM_PVR_CTX_TYPE_RENDER)
		return -EINVAL;

	if (!job->hwrt)
		return -EINVAL;

	job->fw_ccb_cmd_type = ROGUE_FWIF_CCB_CMD_TYPE_GEOM;
	err = pvr_fw_cmd_init(job->pvr_dev, job, &pvr_cmd_geom_stream,
			      args->cmd_stream, args->cmd_stream_len);
	if (err)
		return err;

	cmd = job->cmd;
	cmd->cmd_shared.cmn.frame_num = 0;
	cmd->flags = convert_geom_flags(args->flags);
	pvr_fw_object_get_fw_addr(job->hwrt->fw_obj, &cmd->cmd_shared.hwrt_data_fw_addr);
	return 0;
}

static int
pvr_frag_job_fw_cmd_init(struct pvr_job *job,
			 struct drm_pvr_job *args)
{
	struct rogue_fwif_cmd_frag *cmd;
	int err;

	if (args->flags & ~DRM_PVR_SUBMIT_JOB_FRAG_CMD_FLAGS_MASK)
		return -EINVAL;

	if (job->ctx->type != DRM_PVR_CTX_TYPE_RENDER)
		return -EINVAL;

	if (!job->hwrt)
		return -EINVAL;

	job->fw_ccb_cmd_type = (args->flags & DRM_PVR_SUBMIT_JOB_FRAG_CMD_PARTIAL_RENDER) ?
			       ROGUE_FWIF_CCB_CMD_TYPE_FRAG_PR :
			       ROGUE_FWIF_CCB_CMD_TYPE_FRAG;
	err = pvr_fw_cmd_init(job->pvr_dev, job, &pvr_cmd_frag_stream,
			      args->cmd_stream, args->cmd_stream_len);
	if (err)
		return err;

	cmd = job->cmd;
	cmd->cmd_shared.cmn.frame_num = 0;
	cmd->flags = convert_frag_flags(args->flags);
	pvr_fw_object_get_fw_addr(job->hwrt->fw_obj, &cmd->cmd_shared.hwrt_data_fw_addr);
	return 0;
}

static u32
convert_compute_flags(u32 in_flags)
{
	u32 out_flags = 0;

	if (in_flags & DRM_PVR_SUBMIT_JOB_COMPUTE_CMD_PREVENT_ALL_OVERLAP)
		out_flags |= ROGUE_COMPUTE_FLAG_PREVENT_ALL_OVERLAP;
	if (in_flags & DRM_PVR_SUBMIT_JOB_COMPUTE_CMD_SINGLE_CORE)
		out_flags |= ROGUE_COMPUTE_FLAG_SINGLE_CORE;

	return out_flags;
}

static int
pvr_compute_job_fw_cmd_init(struct pvr_job *job,
			    struct drm_pvr_job *args)
{
	struct rogue_fwif_cmd_compute *cmd;
	int err;

	if (args->flags & ~DRM_PVR_SUBMIT_JOB_COMPUTE_CMD_FLAGS_MASK)
		return -EINVAL;

	if (job->ctx->type != DRM_PVR_CTX_TYPE_COMPUTE)
		return -EINVAL;

	job->fw_ccb_cmd_type = ROGUE_FWIF_CCB_CMD_TYPE_CDM;
	err = pvr_fw_cmd_init(job->pvr_dev, job, &pvr_cmd_compute_stream,
			      args->cmd_stream, args->cmd_stream_len);
	if (err)
		return err;

	cmd = job->cmd;
	cmd->common.frame_num = 0;
	cmd->flags = convert_compute_flags(args->flags);
	return 0;
}

static u32
convert_transfer_flags(u32 in_flags)
{
	u32 out_flags = 0;

	if (in_flags & DRM_PVR_SUBMIT_JOB_TRANSFER_CMD_SINGLE_CORE)
		out_flags |= ROGUE_TRANSFER_FLAGS_SINGLE_CORE;

	return out_flags;
}

static int
pvr_transfer_job_fw_cmd_init(struct pvr_job *job,
			     struct drm_pvr_job *args)
{
	struct rogue_fwif_cmd_transfer *cmd;
	int err;

	if (args->flags & ~DRM_PVR_SUBMIT_JOB_TRANSFER_CMD_FLAGS_MASK)
		return -EINVAL;

	if (job->ctx->type != DRM_PVR_CTX_TYPE_TRANSFER_FRAG)
		return -EINVAL;

	job->fw_ccb_cmd_type = ROGUE_FWIF_CCB_CMD_TYPE_TQ_3D;
	err = pvr_fw_cmd_init(job->pvr_dev, job, &pvr_cmd_transfer_stream,
			      args->cmd_stream, args->cmd_stream_len);
	if (err)
		return err;

	cmd = job->cmd;
	cmd->common.frame_num = 0;
	cmd->flags = convert_transfer_flags(args->flags);
	return 0;
}

static int
pvr_job_fw_cmd_init(struct pvr_job *job,
		    struct drm_pvr_job *args)
{
	switch (args->type) {
	case DRM_PVR_JOB_TYPE_GEOMETRY:
		return pvr_geom_job_fw_cmd_init(job, args);

	case DRM_PVR_JOB_TYPE_FRAGMENT:
		return pvr_frag_job_fw_cmd_init(job, args);

	case DRM_PVR_JOB_TYPE_COMPUTE:
		return pvr_compute_job_fw_cmd_init(job, args);

	case DRM_PVR_JOB_TYPE_TRANSFER_FRAG:
		return pvr_transfer_job_fw_cmd_init(job, args);

	default:
		return -EINVAL;
	}
}


static struct pvr_job *
create_job(struct pvr_device *pvr_dev,
	   struct pvr_file *pvr_file,
	   struct drm_pvr_job *args)
{
	struct pvr_job *job = NULL;
	int err;

	if (!args->cmd_stream || !args->cmd_stream_len)
		return ERR_PTR(-EINVAL);

	if (args->type != DRM_PVR_JOB_TYPE_GEOMETRY &&
	    args->type != DRM_PVR_JOB_TYPE_FRAGMENT &&
	    (args->hwrt.set_handle || args->hwrt.data_index))
		return ERR_PTR(-EINVAL);

	job = kzalloc(sizeof(*job), GFP_KERNEL);
	if (!job)
		return ERR_PTR(-ENOMEM);

	kref_init(&job->ref_count);
	__atomic_add_fetch(&pd_diag_jobs_created, 1, __ATOMIC_RELAXED);
	job->type = args->type;
	job->pvr_dev = pvr_dev;
	INIT_LIST_HEAD(&job->queue_node);

	err = xa_alloc(&pvr_dev->job_ids, &job->id, job, xa_limit_32b, GFP_KERNEL);
	if (err)
		goto err_put_job;

	job->ctx = pvr_context_lookup(pvr_file, args->context_handle);
	if (!job->ctx) {
		err = -EINVAL;
		goto err_put_job;
	}

	if (args->hwrt.set_handle) {
		job->hwrt = pvr_hwrt_data_lookup(pvr_file, args->hwrt.set_handle,
						 args->hwrt.data_index);
		if (!job->hwrt) {
			err = -EINVAL;
			goto err_put_job;
		}
	}

	err = pvr_job_fw_cmd_init(job, args);
	if (err)
		goto err_put_job;

	/* Fragment jobs need at least one native fence wait on the geometry job fence. */
	if (!pvr_cccb_cmdseq_can_fit(&pvr_context_get_queue_for_job(job->ctx, job->type)->cccb,
				     pvr_queue_job_cmds_size(job, job->type == DRM_PVR_JOB_TYPE_FRAGMENT))) {
		err = -E2BIG;
		goto err_put_job;
	}

	return job;

err_put_job:
	pvr_job_put(job);
	return ERR_PTR(err);
}

struct pvr_job_data {
	struct pvr_job *job;
	struct drm_pvr_sync_op *sync_ops;
	u32 sync_op_count;
};

// the fragment job rides along with the geometry job when it waits on what that job signals
static bool can_combine_jobs(struct pvr_job_data *a, struct pvr_job_data *b)
{
	if (a->job->type != DRM_PVR_JOB_TYPE_GEOMETRY ||
	    b->job->type != DRM_PVR_JOB_TYPE_FRAGMENT ||
	    a->job->ctx != b->job->ctx ||
	    a->job->hwrt != b->job->hwrt)
		return false;

	for (u32 i = 0; i < b->sync_op_count; i++) {
		if (b->sync_ops[i].flags & DRM_PVR_SYNC_OP_FLAG_SIGNAL)
			continue;
		for (u32 j = 0; j < a->sync_op_count; j++) {
			if ((a->sync_ops[j].flags & DRM_PVR_SYNC_OP_FLAG_SIGNAL) &&
			    a->sync_ops[j].handle == b->sync_ops[i].handle)
				return true;
		}
	}
	return false;
}

static int
check_sync_ops(struct drm_pvr_sync_op *ops, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		if (ops[i].flags & ~DRM_PVR_SYNC_OP_FLAGS_MASK)
			return -EINVAL;
		// binary syncobjs only; mesa emulates timelines over them
		if ((ops[i].flags & DRM_PVR_SYNC_OP_FLAG_HANDLE_TYPE_MASK) !=
		    DRM_PVR_SYNC_OP_FLAG_HANDLE_TYPE_SYNCOBJ || ops[i].value)
			return -EOPNOTSUPP;
	}
	return 0;
}

// the fences a job waits on: its syncobj waits, and the last job on its hwrt
static int
collect_waits(struct pvr_file *pvr_file, struct pvr_job_data *jd, struct pvr_job_data *skip_from,
	      struct pvr_fence ***waits_out, u32 *count_out)
{
	struct pvr_fence **waits = kcalloc(jd->sync_op_count + 1, sizeof(*waits), GFP_KERNEL);
	u32 n = 0;

	if (!waits)
		return -ENOMEM;
	for (u32 i = 0; i < jd->sync_op_count; i++) {
		struct pvr_fence *f;
		bool internal = false;

		if (jd->sync_ops[i].flags & DRM_PVR_SYNC_OP_FLAG_SIGNAL)
			continue;
		// a wait on what the paired geometry job signals is the partial render fence
		for (u32 j = 0; skip_from && j < skip_from->sync_op_count; j++) {
			if ((skip_from->sync_ops[j].flags & DRM_PVR_SYNC_OP_FLAG_SIGNAL) &&
			    skip_from->sync_ops[j].handle == jd->sync_ops[i].handle)
				internal = true;
		}
		if (internal)
			continue;
		f = pvr_syncobj_fence_get(pvr_file, jd->sync_ops[i].handle);
		if (IS_ERR(f)) {
			while (n)
				pvr_fence_put(waits[--n]);
			kfree(waits);
			return PTR_ERR(f);
		}
		if (f->dependency_count) {
			struct pvr_fence **expanded = kcalloc(n + f->dependency_count + jd->sync_op_count + 1,
							 sizeof(*expanded), GFP_KERNEL);
			if (!expanded) {
				pvr_fence_put(f);
				while (n)
					pvr_fence_put(waits[--n]);
				kfree(waits);
				return -ENOMEM;
			}
			memcpy(expanded, waits, n * sizeof(*waits));
			kfree(waits);
			waits = expanded;
			for (u32 j = 0; j < f->dependency_count; j++)
				waits[n++] = pvr_fence_get(f->dependencies[j]);
			pvr_fence_put(f);
		} else {
			waits[n++] = f;
		}
	}
	if (jd->job->hwrt) {
		struct pvr_fence *f = pvr_hwrt_last_fence_get(jd->job->hwrt);

		if (f)
			waits[n++] = f;
	}
	*waits_out = waits;
	*count_out = n;
	return 0;
}

static void
put_waits(struct pvr_fence **waits, u32 count)
{
	for (u32 i = 0; i < count; i++)
		pvr_fence_put(waits[i]);
	kfree(waits);
}

// fences other than queue fences (cpu-signalled syncobjs) have to be done before the kick
static int
wait_foreign_fences(struct pvr_device *pvr_dev, struct pvr_fence **waits, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		int err;

		if (waits[i]->queue)
			continue;
		err = pvr_fence_wait(pvr_dev, waits[i], 10ULL * 1000 * 1000);
		if (err)
			return err;
	}
	return 0;
}

static void
signal_syncs(struct pvr_file *pvr_file, struct pvr_job_data *jd)
{
	for (u32 i = 0; i < jd->sync_op_count; i++) {
		if (jd->sync_ops[i].flags & DRM_PVR_SYNC_OP_FLAG_SIGNAL)
			pvr_syncobj_replace_fence(pvr_file, jd->sync_ops[i].handle, jd->job->done_fence);
	}
	if (jd->job->hwrt)
		pvr_hwrt_last_fence_set(jd->job->hwrt, jd->job->done_fence);
}

int
pvr_submit_jobs(struct pvr_device *pvr_dev, struct pvr_file *pvr_file,
		struct drm_pvr_ioctl_submit_jobs_args *args)
{
	struct pvr_job_data *job_data = NULL;
	struct drm_pvr_job *job_args;
	u32 count = 0;
	int err;

	if (!args->jobs.count)
		return -EINVAL;

	err = PVR_UOBJ_GET_ARRAY(job_args, &args->jobs);
	if (err)
		return err;

	job_data = kvmalloc_array(args->jobs.count, sizeof(*job_data),
				  GFP_KERNEL | __GFP_ZERO);
	if (!job_data) {
		err = -ENOMEM;
		goto out_free;
	}

	for (; count < args->jobs.count; count++) {
		struct pvr_job *job = create_job(pvr_dev, pvr_file, &job_args[count]);

		err = PTR_ERR_OR_ZERO(job);
		if (err)
			goto out_job_data_cleanup;
		job_data[count].job = job;

		err = PVR_UOBJ_GET_ARRAY(job_data[count].sync_ops, &job_args[count].sync_ops);
		if (err) {
			count++;
			goto out_job_data_cleanup;
		}
		job_data[count].sync_op_count = job_args[count].sync_ops.count;
		err = check_sync_ops(job_data[count].sync_ops, job_data[count].sync_op_count);
		if (err) {
			count++;
			goto out_job_data_cleanup;
		}
	}

	/*
	 * Flush MMU if needed - this has been deferred until now to avoid
	 * overuse of this expensive operation.
	 */
	err = pvr_mmu_flush_exec(pvr_dev, false);
	if (err)
		goto out_job_data_cleanup;

	mutex_lock(&pvr_dev->submit_lock);
	for (u32 i = 0; i < count; i++) {
		struct pvr_job_data *jd = &job_data[i];
		struct pvr_job_data *pair = NULL;
		struct pvr_fence **waits = NULL;
		u32 nwaits = 0;

		if (i + 1 < count && can_combine_jobs(jd, &job_data[i + 1]))
			pair = &job_data[i + 1];

		err = collect_waits(pvr_file, jd, NULL, &waits, &nwaits);
		if (!err && pair) {
			struct pvr_fence **pw = NULL;
			u32 npw = 0;

			// the fragment's own waits join the geometry's, the pair goes in one kick
			err = collect_waits(pvr_file, pair, jd, &pw, &npw);
			if (!err) {
				struct pvr_fence **all = kcalloc(nwaits + npw, sizeof(*all), GFP_KERNEL);

				if (all) {
					memcpy(all, waits, nwaits * sizeof(*all));
					memcpy(all + nwaits, pw, npw * sizeof(*all));
					kfree(waits);
					kfree(pw);
					waits = all;
					nwaits += npw;
				} else {
					put_waits(pw, npw);
					err = -ENOMEM;
				}
			}
		}
		if (!err)
			err = wait_foreign_fences(pvr_dev, waits, nwaits);
		if (!err) {
			if (pair) {
				jd->job->paired_job = pair->job;
				pair->job->paired_job = jd->job;
			}
			err = pvr_queue_submit(jd->job, waits, nwaits);
			if (pair) {
				jd->job->paired_job = NULL;
				pair->job->paired_job = NULL;
			}
		}
		if (waits)
			put_waits(waits, nwaits);
		if (err)
			break;

		signal_syncs(pvr_file, jd);
		if (pair) {
			signal_syncs(pvr_file, pair);
			i++;
		}
	}
	mutex_unlock(&pvr_dev->submit_lock);
	pvr_queue_process_all_try(pvr_dev);

out_job_data_cleanup:
	for (u32 i = 0; i < count; i++) {
		pvr_job_put(job_data[i].job);
		kvfree(job_data[i].sync_ops);
	}

out_free:
	kvfree(job_data);
	kvfree(job_args);

	return err;
}
