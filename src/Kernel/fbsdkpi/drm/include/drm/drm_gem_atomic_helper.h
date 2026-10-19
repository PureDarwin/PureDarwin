// SPDX-License-Identifier: MIT
// GEM atomic helpers, the subset amdgpu uses
#ifndef _PD_DRM_GEM_ATOMIC_HELPER_H_
#define _PD_DRM_GEM_ATOMIC_HELPER_H_

struct drm_plane;
struct drm_plane_state;

int drm_gem_plane_helper_prepare_fb(struct drm_plane *plane,
    struct drm_plane_state *state);

#endif
