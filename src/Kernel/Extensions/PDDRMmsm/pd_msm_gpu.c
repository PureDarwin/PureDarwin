// SPDX-License-Identifier: MIT
// what every adreno generation shares: registers, the iommu, clocks, the first-open start, fences

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/ktime.h>

#include "pd_msm.h"

// the iommu kinds this driver knows, matched against the node the gpu's iommus property names
static const struct pd_msm_iommu *const iommus[] = {
	&pd_msm_iommu_v1,
};

static int
iommu_attach(struct pd_msm_dev *mdev)
{
	struct device *dev = &mdev->pdev->dev;
	struct of_phandle_args args;

	if (of_parse_phandle_with_args(dev->of_node, "iommus", "#iommu-cells", 0, &args) != 0) {
		dev_err(dev, "no iommus property\n");
		return (-ENODEV);
	}
	for (int i = 0; i < ARRAY_SIZE(iommus); i++) {
		if (!of_device_is_compatible(args.np, iommus[i]->compatible))
			continue;
		mdev->gpu.iommu = iommus[i];
		mdev->gpu.iommu_priv = iommus[i]->attach(dev);
		return (mdev->gpu.iommu_priv != NULL ? 0 : -ENODEV);
	}
	dev_err(dev, "no driver for the gpu's iommu\n");
	return (-ENODEV);
}

int
pd_msm_iommu_map(struct pd_msm_dev *mdev, u64 iova, u64 phys, size_t size)
{
	return (mdev->gpu.iommu->map(mdev->gpu.iommu_priv, iova, phys, size));
}

void
pd_msm_iommu_unmap(struct pd_msm_dev *mdev, u64 iova, size_t size)
{
	mdev->gpu.iommu->unmap(mdev->gpu.iommu_priv, iova, size);
}

// spec 2: the core clock at the chip's top rate, then every clock the node lists. The power
// domain came on before probe, the iommu's clocks with the iommu
static int
power_on(struct pd_msm_dev *mdev)
{
	struct device *dev = &mdev->pdev->dev;
	int error;

	mdev->nclks = devm_clk_bulk_get_all(dev, &mdev->clks);
	if (mdev->nclks < 0)
		return (mdev->nclks);
	for (int i = 0; i < mdev->nclks; i++) {
		if (mdev->clks[i].id != NULL && strcmp(mdev->clks[i].id, mdev->info->core_clk) == 0)
			mdev->core_clk = mdev->clks[i].clk;
	}
	if (mdev->core_clk != NULL && (error = clk_set_rate(mdev->core_clk, mdev->info->max_freq)) != 0)
		dev_err(dev, "core clock rate: %d\n", error);
	return (clk_bulk_prepare_enable(mdev->nclks, mdev->clks));
}

// probe time, nothing from disk: the root filesystem may not be there yet
int
pd_msm_gpu_init(struct pd_msm_dev *mdev)
{
	struct pd_msm_gpu *g = &mdev->gpu;
	struct device *dev = &mdev->pdev->dev;
	int error;

	mutex_init(&g->lock);
	mutex_init(&g->start_lock);
	g->va_next = mdev->info->va_start;
	g->mmio = devm_platform_ioremap_resource(mdev->pdev, 0);
	if (IS_ERR(g->mmio)) {
		dev_err(dev, "cannot map the gpu registers: %ld\n", PTR_ERR(g->mmio));
		return (PTR_ERR(g->mmio));
	}
	if ((error = iommu_attach(mdev)) != 0)
		return (error);
	if ((error = power_on(mdev)) != 0) {
		dev_err(dev, "clocks did not start: %d\n", error);
		return (error);
	}
	return (mdev->info->funcs->init(mdev));
}

// runs once, from the first open, when microcode on the root filesystem can be read
int
pd_msm_gpu_start(struct pd_msm_dev *mdev)
{
	struct pd_msm_gpu *g = &mdev->gpu;
	int error = 0;

	mutex_lock(&g->start_lock);
	if (!g->running) {
		error = mdev->info->funcs->start(mdev);
		g->running = error == 0;
	}
	mutex_unlock(&g->start_lock);
	return (error);
}

int
pd_msm_gpu_submit(struct pd_msm_dev *mdev, struct drm_file *file, const u64 *ib_iova,
    const u32 *ib_dwords, int nib, u32 *seqno)
{
	struct pd_msm_gpu *g = &mdev->gpu;
	int error;

	if (!g->running)
		return (-ENODEV);
	mutex_lock(&g->lock);
	*seqno = g->seqno + 1;
	error = mdev->info->funcs->submit(mdev, file, ib_iova, ib_dwords, nib, *seqno);
	if (error == 0) {
		g->seqno = *seqno;
		g->last_ctx = file;
	}
	mutex_unlock(&g->lock);
	return (error);
}

u32
pd_msm_gpu_completed(struct pd_msm_dev *mdev)
{
	return (mdev->info->funcs->completed(mdev));
}

// polled for now, seqnos compare with wraparound
int
pd_msm_gpu_wait(struct pd_msm_dev *mdev, u32 seqno, s64 timeout_ns)
{
	u64 deadline = ktime_get_ns() + (timeout_ns > 0 ? timeout_ns : 0);

	for (;;) {
		if ((s32)(pd_msm_gpu_completed(mdev) - seqno) >= 0)
			break;
		if (timeout_ns <= 0 || ktime_get_ns() >= deadline)
			return (-ETIMEDOUT);
		usleep_range(50, 100);
	}
	if (mdev->info->funcs->fence_ack != NULL)
		mdev->info->funcs->fence_ack(mdev);
	return (0);
}
