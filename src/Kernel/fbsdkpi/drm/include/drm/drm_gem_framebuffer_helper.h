// SPDX-License-Identifier: MIT
// GEM framebuffer helpers, the subset amdgpu uses
#ifndef _PD_DRM_GEM_FRAMEBUFFER_HELPER_H_
#define _PD_DRM_GEM_FRAMEBUFFER_HELPER_H_

struct drm_framebuffer;
struct drm_file;
struct drm_gem_object;

struct drm_gem_object *drm_gem_fb_get_obj(struct drm_framebuffer *fb,
    unsigned int plane);
void drm_gem_fb_destroy(struct drm_framebuffer *fb);
int drm_gem_fb_create_handle(struct drm_framebuffer *fb, struct drm_file *file,
    unsigned int *handle);

#endif
