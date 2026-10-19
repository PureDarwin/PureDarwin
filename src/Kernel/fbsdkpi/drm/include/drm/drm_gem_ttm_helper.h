// SPDX-License-Identifier: MIT
// GEM helpers for TTM objects, the subset amdgpu uses
#ifndef _PD_DRM_GEM_TTM_HELPER_H_
#define _PD_DRM_GEM_TTM_HELPER_H_

#include <linux/container_of.h>

#include <drm/drm_gem.h>
#include <drm/ttm/ttm_bo.h>

struct iosys_map;
struct vm_area_struct;

#define drm_gem_ttm_of_gem(gem_obj) \
	container_of(gem_obj, struct ttm_buffer_object, base)

int drm_gem_ttm_vmap(struct drm_gem_object *gem, struct iosys_map *map);
void drm_gem_ttm_vunmap(struct drm_gem_object *gem, struct iosys_map *map);
int drm_gem_ttm_mmap(struct drm_gem_object *gem, struct vm_area_struct *vma);
int drm_gem_ttm_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *offset);

#endif
