// pd_msm.h - clean-room adreno drm driver (msm uapi): shared state, generations and chips, from the adreno-cleanroom spec
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#ifndef PD_MSM_H
#define PD_MSM_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/platform_device.h>

#include <drm/drm_device.h>
#include <drm/drm_gem.h>
#include <drm/drm_file.h>

// a physically contiguous piece of a bo, the smmu joins the pieces
struct pd_msm_chunk {
	void			*alloc;		// the allocator's own cacheable view, never used after setup
	u64			phys;
	size_t			size;
};

struct pd_msm_bo {
	struct drm_gem_object	base;
	void			*cpu;		// kernel bos only (ring, memptrs): an uncached view
	struct pd_msm_chunk	*chunks;
	u32			nchunks;
	u64			iova;		// 0 until the gpu needs it
	u32			flags;
	u32			last_fence;	// seqno of the last submit that used it
	char			name[32];
};

#define to_pd_msm_bo(obj)	container_of((obj), struct pd_msm_bo, base)

struct pd_msm_dev;
struct drm_file;

// an iommu kind the gpu can sit behind, picked by the compatible of the node its iommus names
struct pd_msm_iommu {
	const char		*compatible;
	void			*(*attach)(struct device *dev);
	int			(*map)(void *priv, u64 iova, u64 phys, size_t size);
	void			(*unmap)(void *priv, u64 iova, size_t size);
	void			(*dump_fault)(void *priv);
};

// one gpu generation: what differs between a3xx, a5xx, a6xx and on
struct pd_msm_gpu_funcs {
	// probe time, after registers, iommu and clocks: version check and the kernel's own buffers
	int			(*init)(struct pd_msm_dev *mdev);
	// first open: microcode, register init and cp start
	int			(*start)(struct pd_msm_dev *mdev);
	// one submit's indirect buffers and its fence, the seqno back
	int			(*submit)(struct pd_msm_dev *mdev, struct drm_file *file, const u64 *ib_iova,
				    const u32 *ib_dwords, int nib, u32 seqno);
	// the last seqno the gpu finished
	u32			(*completed)(struct pd_msm_dev *mdev);
	// after a wait: clears what the fence raised, interrupts are not wired yet
	void			(*fence_ack)(struct pd_msm_dev *mdev);
	void			(*dump)(struct pd_msm_dev *mdev);
};

// one chip, from the compatible's match data: what userspace is told and how it is driven
struct pd_msm_info {
	const char		*name;
	u32			gpu_id;
	u64			chip_id;
	u32			gmem_size;
	u64			gmem_base;
	u32			max_freq;
	const char		*core_clk;	// the clock run at max_freq
	u64			va_start, va_end;
	const struct pd_msm_gpu_funcs *funcs;
};

struct pd_msm_gpu {
	void __iomem		*mmio;
	const struct pd_msm_iommu *iommu;
	void			*iommu_priv;
	struct pd_msm_bo	*ring;
	struct pd_msm_bo	*memptrs;	// fence word at offset 0
	u32			wptr;		// in dwords
	u32			seqno;		// last one handed out
	struct mutex		lock;		// ring, seqno and the gpu va allocator
	struct mutex		start_lock;	// the first open's microcode load and cp start
	u64			va_next;
	struct drm_file		*last_ctx;
	bool			running;
};

struct pd_msm_dev {
	struct drm_device	drm;
	struct platform_device	*pdev;
	const struct pd_msm_info *info;
	struct clk_bulk_data	*clks;
	int			nclks;
	struct clk		*core_clk;
	struct pd_msm_gpu	gpu;
};

#define to_pd_msm(d)	container_of((d), struct pd_msm_dev, drm)

// pd_msm_bo.c: physically contiguous memory below 4 GB, returned as an uncached kernel mapping
void *pd_msm_dma_alloc(size_t size, u64 *phys, void **alloc);
void pd_msm_dma_free(void *cpu, void *alloc, size_t size);

// pd_msm_bo.c
// a kernel bo is one contiguous piece with a cpu view, a user one any pieces and none
#define PD_MSM_BO_KERNEL	0x80000000u
struct pd_msm_bo *pd_msm_bo_new(struct drm_device *drm, size_t size, u32 flags);
int pd_msm_bo_iova(struct pd_msm_dev *mdev, struct pd_msm_bo *bo);
void pd_msm_mmap_register(void);
extern const struct drm_gem_object_funcs pd_msm_bo_funcs;

// pd_msm_gpu.c: what every generation shares
int pd_msm_gpu_init(struct pd_msm_dev *mdev);
int pd_msm_gpu_start(struct pd_msm_dev *mdev);
int pd_msm_gpu_submit(struct pd_msm_dev *mdev, struct drm_file *file, const u64 *ib_iova,
    const u32 *ib_dwords, int nib, u32 *seqno);
u32 pd_msm_gpu_completed(struct pd_msm_dev *mdev);
// waits for seqno up to timeout_ns, 0 or -ETIMEDOUT
int pd_msm_gpu_wait(struct pd_msm_dev *mdev, u32 seqno, s64 timeout_ns);
int pd_msm_iommu_map(struct pd_msm_dev *mdev, u64 iova, u64 phys, size_t size);
void pd_msm_iommu_unmap(struct pd_msm_dev *mdev, u64 iova, size_t size);

// the generations
extern const struct pd_msm_gpu_funcs pd_msm_a3xx_funcs;

// the iommus: pd_msm_smmu.c drives msm8916's qcom msm-iommu-v1
extern const struct pd_msm_iommu pd_msm_iommu_v1;

// the soc clock and power providers: pd_msm_gcc8916.c
void pd_msm_gcc8916_register(void);

#endif
