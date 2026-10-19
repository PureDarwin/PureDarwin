// SPDX-License-Identifier: MIT
// a ttm resource backed by nvkm memory, with the two mappings a copy uses
#ifndef __NOUVEAU_MEM_H__
#define __NOUVEAU_MEM_H__

#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_resource.h>
#include <drm/ttm/ttm_tt.h>
#include <drm/ttm/ttm_placement.h>

#include <nvif/mem.h>
#include <nvif/vmm.h>

struct nouveau_drm;

struct nouveau_mem {
	struct ttm_resource base;
	struct nouveau_drm *drm;
	u8 kind;
	u8 comp;
	struct nvif_mem mem;
	struct nvif_vma vma[2];
};

static inline struct nouveau_mem *
nouveau_mem(struct ttm_resource *reg)
{
	return container_of(reg, struct nouveau_mem, base);
}

int nouveau_mem_new(struct nouveau_drm *drm, u8 kind, u8 comp, struct ttm_resource **res);
void nouveau_mem_del(struct ttm_resource_manager *man, struct ttm_resource *reg);
int nouveau_mem_vram(struct ttm_resource *reg, bool contig, u8 page);
int nouveau_mem_host(struct ttm_resource *reg, struct ttm_tt *tt);
void nouveau_mem_fini(struct nouveau_mem *mem);
int nouveau_mem_map(struct nouveau_mem *mem, struct nvif_vmm *vmm, struct nvif_vma *vma);
bool nouveau_mem_intersects(struct ttm_resource *res, const struct ttm_place *place, size_t size);
bool nouveau_mem_compatible(struct ttm_resource *res, const struct ttm_place *place, size_t size);

#endif
