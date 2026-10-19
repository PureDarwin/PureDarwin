// SPDX-License-Identifier: MIT
// the msm drm uapi (spec msm-uapi.md): device, buffers, submits and fences, the chips it drives

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_vma_manager.h>
#include <drm/msm_drm.h>

#include "pd_msm.h"

// the chips, what each reports to userspace as the reference driver does and its generation
// (observations/msm-uapi-params.txt). adding one is an entry here and a compatible below
static const struct pd_msm_info a306_info = {
	.name = "adreno 306",
	.gpu_id = 307,
	.chip_id = 0x03000600ull,
	.gmem_size = 0x20000,
	.gmem_base = 0x100000,
	.max_freq = 400000000,
	.core_clk = "gfx3d",
	// a3xx addresses are 32 bits, the kernel's own buffers sit at the top
	.va_start = 0x01000000ull,
	.va_end = 0xfff00000ull,
	.funcs = &pd_msm_a3xx_funcs,
};

struct pd_msm_file {
	u32	next_queue;
};

static int
get_param(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_param *p = data;
	const struct pd_msm_info *info = to_pd_msm(drm)->info;

	if (p->pipe != MSM_PIPE_3D0 || p->pad != 0)
		return (-EINVAL);
	switch (p->param) {
	case MSM_PARAM_GPU_ID:		p->value = info->gpu_id; break;
	case MSM_PARAM_GMEM_SIZE:	p->value = info->gmem_size; break;
	case MSM_PARAM_CHIP_ID:		p->value = info->chip_id; break;
	case MSM_PARAM_MAX_FREQ:	p->value = info->max_freq; break;
	case MSM_PARAM_GMEM_BASE:	p->value = info->gmem_base; break;
	case MSM_PARAM_PRIORITIES:	p->value = 1; break;
	case MSM_PARAM_PP_PGTABLE:	p->value = 0; break;
	case MSM_PARAM_FAULTS:		p->value = 0; break;
	case MSM_PARAM_SUSPENDS:	p->value = 0; break;
	default:
		return (-EINVAL);
	}
	return (0);
}

static int
set_param(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_param *p = data;

	if (p->pipe != MSM_PIPE_3D0)
		return (-EINVAL);
	// accepted and ignored: profiling and the task name only matter to debugging tools
	switch (p->param) {
	case MSM_PARAM_SYSPROF:
	case MSM_PARAM_COMM:
	case MSM_PARAM_CMDLINE:
		return (0);
	default:
		return (-EINVAL);
	}
}

static int
gem_new(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_gem_new *req = data;
	struct pd_msm_bo *bo;
	int error;

	if ((req->flags & ~MSM_BO_FLAGS) != 0)
		return (-EINVAL);
	bo = pd_msm_bo_new(drm, req->size, req->flags);
	if (IS_ERR(bo))
		return (PTR_ERR(bo));
	error = drm_gem_handle_create(file, &bo->base, &req->handle);
	drm_gem_object_put(&bo->base);
	return (error);
}

static int
gem_info(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_gem_info *req = data;
	struct drm_gem_object *obj;
	struct pd_msm_bo *bo;
	int error = 0;

	if (req->pad != 0)
		return (-EINVAL);
	obj = drm_gem_object_lookup(file, req->handle);
	if (obj == NULL)
		return (-ENOENT);
	bo = to_pd_msm_bo(obj);
	switch (req->info) {
	case MSM_INFO_GET_OFFSET:
		req->value = drm_vma_node_offset_addr(&obj->vma_node);
		break;
	case MSM_INFO_GET_IOVA:
		error = pd_msm_bo_iova(to_pd_msm(drm), bo);
		req->value = bo->iova;
		break;
	case MSM_INFO_GET_FLAGS:
		req->value = bo->flags;
		break;
	case MSM_INFO_SET_NAME: {
		char name[sizeof(bo->name)] = { 0 };
		size_t n = min_t(size_t, req->len, sizeof(name) - 1);
		if (copy_from_user(name, u64_to_user_ptr(req->value), n) != 0)
			error = -EFAULT;
		else
			memcpy(bo->name, name, sizeof(name));
		break;
	}
	case MSM_INFO_GET_NAME: {
		size_t n = strlen(bo->name);
		if (req->value != 0 && req->len >= n &&
		    copy_to_user(u64_to_user_ptr(req->value), bo->name, n) != 0)
			error = -EFAULT;
		req->len = n;
		break;
	}
	default:
		// metadata and fixed iovas are not offered by this version of the uapi
		error = -EINVAL;
	}
	drm_gem_object_put(obj);
	return (error);
}

// the absolute monotonic deadline userspace passes, as a relative wait
static s64
deadline_ns(const struct drm_msm_timespec *ts)
{
	s64 abs = ts->tv_sec * 1000000000ll + ts->tv_nsec;

	return (abs - (s64)ktime_get_ns());
}

static int
gem_cpu_prep(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_gem_cpu_prep *req = data;
	struct drm_gem_object *obj;
	struct pd_msm_dev *mdev = to_pd_msm(drm);
	int error = 0;
	u32 fence;

	if ((req->op & ~(MSM_PREP_READ | MSM_PREP_WRITE | MSM_PREP_NOSYNC | MSM_PREP_BOOST)) != 0)
		return (-EINVAL);
	obj = drm_gem_object_lookup(file, req->handle);
	if (obj == NULL)
		return (-ENOENT);
	fence = to_pd_msm_bo(obj)->last_fence;
	if (fence != 0 && (s32)(pd_msm_gpu_completed(mdev) - fence) < 0) {
		if ((req->op & MSM_PREP_NOSYNC) != 0)
			error = -EBUSY;
		else
			error = pd_msm_gpu_wait(mdev, fence, deadline_ns(&req->timeout));
	}
	drm_gem_object_put(obj);
	return (error);
}

static int
gem_cpu_fini(struct drm_device *drm, void *data, struct drm_file *file)
{
	// the cpu view is uncached, there is nothing to write back
	return (0);
}

#define MAX_SUBMIT_BOS	4096
#define MAX_SUBMIT_CMDS	64

static int
gem_submit(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_gem_submit *req = data;
	struct pd_msm_dev *mdev = to_pd_msm(drm);
	struct drm_msm_gem_submit_bo *ubos = NULL;
	struct drm_msm_gem_submit_cmd *ucmds = NULL;
	struct pd_msm_bo **bos = NULL;
	u64 ib_iova[MAX_SUBMIT_CMDS];
	u32 ib_dwords[MAX_SUBMIT_CMDS];
	int nib = 0, error = 0;
	u32 seqno;

	if ((req->flags & MSM_PIPE_ID_MASK) != MSM_PIPE_3D0)
		return (-EINVAL);
	// fence fds, syncobjs and userspace seqnos are not offered yet
	if ((req->flags & ~(MSM_PIPE_ID_MASK | MSM_SUBMIT_NO_IMPLICIT)) != 0)
		return (-EINVAL);
	if (req->nr_bos > MAX_SUBMIT_BOS || req->nr_cmds > MAX_SUBMIT_CMDS)
		return (-EINVAL);

	ubos = kvmalloc_array(req->nr_bos, sizeof(*ubos), GFP_KERNEL);
	ucmds = kvmalloc_array(req->nr_cmds, sizeof(*ucmds), GFP_KERNEL);
	bos = kvzalloc(req->nr_bos * sizeof(*bos), GFP_KERNEL);
	if ((req->nr_bos != 0 && (ubos == NULL || bos == NULL)) || (req->nr_cmds != 0 && ucmds == NULL)) {
		error = -ENOMEM;
		goto out;
	}
	if (copy_from_user(ubos, u64_to_user_ptr(req->bos), req->nr_bos * sizeof(*ubos)) != 0 ||
	    copy_from_user(ucmds, u64_to_user_ptr(req->cmds), req->nr_cmds * sizeof(*ucmds)) != 0) {
		error = -EFAULT;
		goto out;
	}
	for (u32 i = 0; i < req->nr_bos; i++) {
		struct drm_gem_object *obj;

		if ((ubos[i].flags & ~MSM_SUBMIT_BO_FLAGS) != 0) {
			error = -EINVAL;
			goto out;
		}
		obj = drm_gem_object_lookup(file, ubos[i].handle);
		if (obj == NULL) {
			error = -ENOENT;
			goto out;
		}
		bos[i] = to_pd_msm_bo(obj);
		if ((error = pd_msm_bo_iova(mdev, bos[i])) != 0)
			goto out;
	}
	for (u32 i = 0; i < req->nr_cmds; i++) {
		struct drm_msm_gem_submit_cmd *c = &ucmds[i];
		struct pd_msm_bo *bo;

		// softpin: buffers are addressed by their iovas, never patched
		if (c->nr_relocs != 0 || c->submit_idx >= req->nr_bos || (c->size & 3) != 0) {
			error = -EINVAL;
			goto out;
		}
		bo = bos[c->submit_idx];
		if ((u64)c->submit_offset + c->size > bo->base.size) {
			error = -EINVAL;
			goto out;
		}
		switch (c->type) {
		case MSM_SUBMIT_CMD_BUF:
			break;
		case MSM_SUBMIT_CMD_CTX_RESTORE_BUF:
			// only needed when another context ran in between
			if (mdev->gpu.last_ctx == file)
				continue;
			break;
		case MSM_SUBMIT_CMD_IB_TARGET_BUF:
			continue;
		default:
			error = -EINVAL;
			goto out;
		}
		ib_iova[nib] = bo->iova + c->submit_offset;
		ib_dwords[nib] = c->size / 4;
		nib++;
	}
	error = pd_msm_gpu_submit(mdev, file, ib_iova, ib_dwords, nib, &seqno);
	if (error == 0) {
		for (u32 i = 0; i < req->nr_bos; i++)
			bos[i]->last_fence = seqno;
		req->fence = seqno;
	}
out:
	for (u32 i = 0; bos != NULL && i < req->nr_bos; i++) {
		if (bos[i] != NULL)
			drm_gem_object_put(&bos[i]->base);
	}
	kvfree(bos);
	kvfree(ucmds);
	kvfree(ubos);
	return (error);
}

static int
wait_fence(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_wait_fence *req = data;

	// boost is only a frequency hint, there is one ring, so the queue id does not matter
	if ((req->flags & ~MSM_WAIT_FENCE_BOOST) != 0)
		return (-EINVAL);
	return (pd_msm_gpu_wait(to_pd_msm(drm), req->fence, deadline_ns(&req->timeout)));
}

static int
gem_madvise(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_gem_madvise *req = data;
	struct drm_gem_object *obj = drm_gem_object_lookup(file, req->handle);

	if (obj == NULL)
		return (-ENOENT);
	// buffers are never purged, so the contents are always still there
	req->retained = 1;
	drm_gem_object_put(obj);
	return (0);
}

static int
submitqueue_new(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_submitqueue *req = data;
	struct pd_msm_file *f = file->driver_priv;

	if (req->prio != 0)
		return (-EINVAL);
	// one ring: every queue runs on it in submission order
	req->id = ++f->next_queue;
	return (0);
}

static int
submitqueue_close(struct drm_device *drm, void *data, struct drm_file *file)
{
	return (0);
}

static int
submitqueue_query(struct drm_device *drm, void *data, struct drm_file *file)
{
	struct drm_msm_submitqueue_query *req = data;
	u32 faults = 0;

	if (req->param != MSM_SUBMITQUEUE_PARAM_FAULTS)
		return (-EINVAL);
	if (req->data != 0 && req->len >= sizeof(faults) &&
	    copy_to_user(u64_to_user_ptr(req->data), &faults, sizeof(faults)) != 0)
		return (-EFAULT);
	req->len = sizeof(faults);
	return (0);
}

static const struct drm_ioctl_desc pd_msm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MSM_GET_PARAM, get_param, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SET_PARAM, set_param, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_NEW, gem_new, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_INFO, gem_info, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_CPU_PREP, gem_cpu_prep, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_CPU_FINI, gem_cpu_fini, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_SUBMIT, gem_submit, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_WAIT_FENCE, wait_fence, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_GEM_MADVISE, gem_madvise, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SUBMITQUEUE_NEW, submitqueue_new, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SUBMITQUEUE_CLOSE, submitqueue_close, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MSM_SUBMITQUEUE_QUERY, submitqueue_query, DRM_RENDER_ALLOW),
};

// the gpu starts on its first user, when /Library/Firmware is surely mounted
static int
pd_msm_open(struct drm_device *drm, struct drm_file *file)
{
	int error = pd_msm_gpu_start(to_pd_msm(drm));

	if (error != 0)
		return (error);
	file->driver_priv = kzalloc(sizeof(struct pd_msm_file), GFP_KERNEL);
	return (file->driver_priv != NULL ? 0 : -ENOMEM);
}

static void
pd_msm_postclose(struct drm_device *drm, struct drm_file *file)
{
	struct pd_msm_dev *mdev = to_pd_msm(drm);

	if (mdev->gpu.last_ctx == file)
		mdev->gpu.last_ctx = NULL;
	kfree(file->driver_priv);
}

DEFINE_DRM_GEM_FOPS(pd_msm_fops);

static const struct drm_driver pd_msm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_RENDER,
	.open = pd_msm_open,
	.postclose = pd_msm_postclose,
	.ioctls = pd_msm_ioctls,
	.num_ioctls = ARRAY_SIZE(pd_msm_ioctls),
	.fops = &pd_msm_fops,
	.name = "msm",
	.desc = "Adreno for PureDarwin",
	// softpin and robustness, spec msm-uapi.md
	.major = 1,
	.minor = 5,
};

static int
pd_msm_probe(struct platform_device *pdev)
{
	struct pd_msm_dev *mdev;
	int error;

	mdev = devm_drm_dev_alloc(&pdev->dev, &pd_msm_driver, struct pd_msm_dev, drm);
	if (IS_ERR(mdev))
		return (PTR_ERR(mdev));
	mdev->pdev = pdev;
	mdev->info = of_device_get_match_data(&pdev->dev);
	if (mdev->info == NULL)
		return (-ENODEV);
	platform_set_drvdata(pdev, mdev);
	if ((error = pd_msm_gpu_init(mdev)) != 0) {
		dev_err(&pdev->dev, "gpu init failed: %d\n", error);
		return (error);
	}
	pd_msm_mmap_register();
	if ((error = drm_dev_register(&mdev->drm, 0)) != 0) {
		dev_err(&pdev->dev, "drm_dev_register: %d\n", error);
		return (error);
	}
	dev_info(&pdev->dev, "%s registered, the cp starts on first open\n", mdev->info->name);
	return (0);
}

static const struct of_device_id pd_msm_match[] = {
	{ .compatible = "qcom,adreno-306.0", .data = &a306_info },
	{ }
};

static struct platform_driver pd_msm_platform_driver = {
	.probe = pd_msm_probe,
	.driver = { .name = "pd-msm", .of_match_table = pd_msm_match },
};

// each soc's clock and power provider goes in before the gpu can probe, they match by
// compatible so the others stay idle (sc8280xp's would sit next to msm8916's)
static int __init
pd_msm_init(void)
{
	pd_msm_gcc8916_register();
	return (platform_driver_register(&pd_msm_platform_driver));
}
module_init(pd_msm_init);
