// fake_msm.c - pd_msm_bo.c's allocators as the host tests provide them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#include "fake.h"
#include "pd_msm.h"

u64 pd_msm_dma_max = ~0ull;
long fake_bos;          // pd_msm_bo_new objects alive
u64 fake_iova_next = 0x100000000ull;

void *
pd_msm_dma_alloc(size_t size, u64 *phys, void **alloc)
{
    size = round_up(size, PAGE_SIZE);
    *alloc = pdos_malloc_contig(size, PAGE_SIZE, pd_msm_dma_max, phys);
    return (*alloc);
}

void
pd_msm_dma_free(void *cpu, void *alloc, size_t size)
{
    pdos_free_contig(alloc, round_up(size, PAGE_SIZE));
}

struct pd_msm_bo *
pd_msm_bo_new(struct drm_device *drm, size_t size, u32 flags)
{
    struct pd_msm_bo *bo = kzalloc(sizeof(*bo), 0);

    size = round_up(size, 16384);
    bo->chunks = kzalloc(sizeof(*bo->chunks), 0);
    bo->cpu = pd_msm_dma_alloc(size, &bo->chunks[0].phys, &bo->chunks[0].alloc);
    bo->chunks[0].size = size;
    bo->nchunks = 1;
    bo->base.size = size;
    bo->base.refcount = 1;
    bo->base.dev = drm;
    fake_bos++;
    return (bo);
}

int
pd_msm_bo_iova(struct pd_msm_dev *mdev, struct pd_msm_bo *bo)
{
    if (bo->iova == 0) {
        bo->iova = fake_iova_next;
        fake_iova_next += bo->base.size;
    }
    return (0);
}

void
drm_gem_object_put(struct drm_gem_object *obj)
{
    struct pd_msm_bo *bo = to_pd_msm_bo(obj);

    if (--obj->refcount > 0)
        return;
    pd_msm_dma_free(bo->cpu, bo->chunks[0].alloc, obj->size);
    kfree(bo->chunks);
    kfree(bo);
    fake_bos--;
}
