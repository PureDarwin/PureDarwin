#include "pvr_context.h"
#include "pvr_device.h"
#include "pvr_drv.h"
#include "pvr_gem.h"
#include "pvr_queue.h"
#include "pvr_stream.h"
#include "pvr_stream_defs.h"
#include "pvr_sync.h"
#include "pvr_vm.h"

#include "pvr_rogue_fwif_client.h"
#include "pvr_rogue_fwif_stream.h"

// a boot-time run of the whole job path through the ioctl layer, as a client would drive it: a vm context, buffers,
// a compute context and one compute job whose cdm control stream is a lone stream terminate, so the firmware runs the kick and finishes without a kernel

#define ST_CTRL_ADDR_OFF	0x200000ULL
#define ST_STATE_ADDR_OFF	0x300000ULL
// mesa's csbgen cdm.xml: STREAM_TERMINATE is block type 2 in bits 30-31
#define ST_CDM_STREAM_TERMINATE	0x80000000u

static int
put(u8 *buf, u32 *off, u32 cap, const u8 *src, u32 size, u32 align)
{
	*off = ALIGN(*off, align);
	if (*off + size > cap)
		return -E2BIG;
	memcpy(buf + *off, src, size);
	*off += size;
	return 0;
}

// the inverse of pvr_stream_process_1(): the supported fields of a filled firmware struct, in order
static int
build_1(struct pvr_device *pvr_dev, const struct pvr_stream_def *defs, u32 n, const u8 *src,
	u8 *buf, u32 *off, u32 cap)
{
	for (u32 i = 0; i < n; i++) {
		const struct pvr_stream_def *d = &defs[i];
		bool ok = d->feature == PVR_FEATURE_NONE ||
			  (!(d->feature & PVR_FEATURE_NOT) && pvr_device_has_feature(pvr_dev, d->feature)) ||
			  ((d->feature & PVR_FEATURE_NOT) &&
			   !pvr_device_has_feature(pvr_dev, d->feature & ~PVR_FEATURE_NOT));
		int err = 0;

		if (!ok)
			continue;
		switch (d->size) {
		case PVR_STREAM_SIZE_8:
			err = put(buf, off, cap, src + d->offset, 1, 1);
			break;
		case PVR_STREAM_SIZE_16:
			err = put(buf, off, cap, src + d->offset, 2, 2);
			break;
		case PVR_STREAM_SIZE_32:
			err = put(buf, off, cap, src + d->offset, 4, 4);
			break;
		case PVR_STREAM_SIZE_64:
			err = put(buf, off, cap, src + d->offset, 8, 8);
			break;
		case PVR_STREAM_SIZE_ARRAY:
			err = put(buf, off, cap, src + d->offset, d->array_size, 8);
			break;
		}
		if (err)
			return err;
	}
	return 0;
}

static int
stream_build(struct pvr_device *pvr_dev, const struct pvr_stream_cmd_defs *cmd_defs,
	     const void *src, u8 *buf, u32 cap, u32 *len_out)
{
	u32 off = 8, hdr[2] = { 0, 0 };
	int err;

	if (cap < 8)
		return -E2BIG;
	err = build_1(pvr_dev, cmd_defs->main_stream, cmd_defs->main_stream_len, src, buf, &off, cap);
	if (err)
		return err;
	hdr[0] = off;
	memcpy(buf, hdr, sizeof(hdr));

	// the extension headers the device's quirks make mandatory, each with its streams
	for (u32 t = 0; t < cmd_defs->ext_nr_headers; t++) {
		u32 mask = pvr_dev->stream_musthave_quirks[cmd_defs->type][t];
		const struct pvr_stream_ext_header *h = &cmd_defs->ext_headers[t];
		u32 ext = (t << PVR_STREAM_EXTHDR_TYPE_SHIFT) | mask;

		if (!mask)
			continue;
		err = put(buf, &off, cap, (const u8 *)&ext, 4, 4);
		for (u32 i = 0; !err && i < h->ext_streams_num; i++) {
			if (ext & h->ext_streams[i].header_mask)
				err = build_1(pvr_dev, h->ext_streams[i].stream, h->ext_streams[i].stream_len,
					      src, buf, &off, cap);
		}
		if (err)
			return err;
	}
	*len_out = off;
	return 0;
}

static int
ioctl(struct pvr_device *pvr_dev, struct pvr_file *f, u32 nr, void *args)
{
	return pvr_ioctl(pvr_dev, f, nr, args, pvr_ioctl_args_size(nr));
}

int
pvr_selftest_null_compute(struct pvr_device *pvr_dev)
{
	struct pvr_file *f = pvr_file_open(pvr_dev);
	struct drm_pvr_ioctl_create_vm_context_args vm = { 0 };
	struct drm_pvr_ioctl_create_bo_args bo_ctrl = { 0 }, bo_state = { 0 };
	struct drm_pvr_ioctl_vm_map_args map = { 0 };
	struct drm_pvr_ioctl_create_context_args ctx = { 0 };
	struct drm_pvr_ioctl_submit_jobs_args submit = { 0 };
	struct rogue_fwif_cdm_registers_cswitch cswitch = { 0 };
	struct rogue_fwif_cmd_compute cmd = { 0 };
	struct drm_pvr_job job = { 0 };
	struct drm_pvr_sync_op sig = { 0 };
	struct drm_pvr_heap heaps[DRM_PVR_HEAP_COUNT];
	struct pvr_gem_object *obj;
	u8 static_state[128], cmd_stream[256];
	u32 static_len = 0, cmd_len = 0, sync = 0, first = 0;
	u64 general, t0;
	u32 *ctrl;
	int err;

	if (!f)
		return -ENOMEM;
	pvr_heaps_get(pvr_dev, heaps, DRM_PVR_HEAP_COUNT);
	general = heaps[DRM_PVR_HEAP_GENERAL].base;

	err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_CREATE_VM_CONTEXT, &vm);
	if (err) {
		pd_pvr_log("selftest: vm context (%d)\n", err);
		goto out;
	}

	bo_ctrl.size = PAGE_SIZE;
	bo_ctrl.flags = DRM_PVR_BO_ALLOW_CPU_USERSPACE_ACCESS;
	bo_state.size = PAGE_SIZE;
	err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_CREATE_BO, &bo_ctrl);
	if (!err)
		err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_CREATE_BO, &bo_state);
	if (err) {
		pd_pvr_log("selftest: buffers (%d)\n", err);
		goto out;
	}
	obj = pvr_gem_object_from_handle(f, bo_ctrl.handle);
	ctrl = obj ? pvr_gem_object_vmap(obj) : NULL;
	if (IS_ERR_OR_NULL(ctrl)) {
		pvr_gem_object_put(obj);
		err = -ENOMEM;
		goto out;
	}
	ctrl[0] = ST_CDM_STREAM_TERMINATE;
	wmb();
	pvr_gem_object_put(obj);

	map.vm_context_handle = vm.handle;
	map.handle = bo_ctrl.handle;
	map.device_addr = general + ST_CTRL_ADDR_OFF;
	map.size = PAGE_SIZE;
	err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_VM_MAP, &map);
	if (!err) {
		map.handle = bo_state.handle;
		map.device_addr = general + ST_STATE_ADDR_OFF;
		err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_VM_MAP, &map);
	}
	if (err) {
		pd_pvr_log("selftest: vm map (%d)\n", err);
		goto out;
	}

	// no context switch programs: the job never yields
	err = stream_build(pvr_dev, &pvr_static_compute_context_state_stream, &cswitch, static_state,
			   sizeof(static_state), &static_len);
	if (err)
		goto out;
	ctx.type = DRM_PVR_CTX_TYPE_COMPUTE;
	ctx.priority = DRM_PVR_CTX_PRIORITY_NORMAL;
	ctx.static_context_state = (u64)(uintptr_t)static_state;
	ctx.static_context_state_len = static_len;
	ctx.vm_context_handle = vm.handle;
	err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_CREATE_CONTEXT, &ctx);
	if (err) {
		pd_pvr_log("selftest: compute context (%d)\n", err);
		goto out;
	}

	cmd.regs.cdm_ctrl_stream_base = general + ST_CTRL_ADDR_OFF;
	cmd.regs.cdm_context_state_base_addr = general + ST_STATE_ADDR_OFF;
	err = stream_build(pvr_dev, &pvr_cmd_compute_stream, &cmd, cmd_stream, sizeof(cmd_stream),
			   &cmd_len);
	if (err)
		goto out;

	err = pvr_syncobj_create(f, 0, &sync);
	if (err)
		goto out;
	sig.handle = sync;
	sig.flags = DRM_PVR_SYNC_OP_FLAG_HANDLE_TYPE_SYNCOBJ | DRM_PVR_SYNC_OP_FLAG_SIGNAL;

	job.type = DRM_PVR_JOB_TYPE_COMPUTE;
	job.context_handle = ctx.handle;
	job.cmd_stream = (u64)(uintptr_t)cmd_stream;
	job.cmd_stream_len = cmd_len;
	job.sync_ops.stride = sizeof(sig);
	job.sync_ops.count = 1;
	job.sync_ops.array = (u64)(uintptr_t)&sig;
	submit.jobs.stride = sizeof(job);
	submit.jobs.count = 1;
	submit.jobs.array = (u64)(uintptr_t)&job;

	// submits the kernel must turn away cleanly: a stream cut short, then a context that is not there
	{
		int bad_len, bad_ctx;

		job.cmd_stream_len = cmd_len - 4;
		bad_len = ioctl(pvr_dev, f, DRM_IOCTL_PVR_SUBMIT_JOBS, &submit);
		job.cmd_stream_len = cmd_len;
		job.context_handle = ctx.handle + 100;
		bad_ctx = ioctl(pvr_dev, f, DRM_IOCTL_PVR_SUBMIT_JOBS, &submit);
		job.context_handle = ctx.handle;
		pd_pvr_log("selftest: bad submits rejected: short stream %d, unknown context %d\n", bad_len, bad_ctx);
	}

	t0 = pd_pvr_now_us();
	err = ioctl(pvr_dev, f, DRM_IOCTL_PVR_SUBMIT_JOBS, &submit);
	if (err) {
		pd_pvr_log("selftest: submit (%d), cmd stream %u bytes, static state %u bytes\n", err,
			   cmd_len, static_len);
		goto out;
	}
	err = pvr_syncobj_wait(pvr_dev, f, &sync, 1, PVR_SYNCOBJ_WAIT_ALL, 2000000, &first);
	pd_pvr_log("selftest: null compute job %s (%d) in %llu us, cmd stream %u bytes, %u interrupts, "
		   "%u fwccb\n", err ? "did not finish" : "finished", err, pd_pvr_now_us() - t0,
		   cmd_len, pvr_dev->irq_count, pvr_dev->fwccb_count);

out:
	pvr_file_close(f);
	return err;
}
