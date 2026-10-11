#ifndef AIRJITD_VK_H
#define AIRJITD_VK_H

#include <stdint.h>

#include "../bridge.h"

// the Vulkan executor: a job's shaders through air2spv, run on whatever Vulkan device the loader
// finds. AIRJITD_VULKAN=1 turns it on, AIRJITD_VK_PROFILE=pvr holds it to PowerVR's limits

// the job is not for this executor (feature, limit or translation): run it on the CPU
#define PD_VK_FALLBACK 1

int pd_vk_enabled(void);
// a submit returned VK_ERROR_DEVICE_LOST: no later job can run on this device
int pd_vk_device_lost(void);

// 0 done (outputs written into base), PD_VK_FALLBACK, or a negative daemon error
int pd_vk_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t bsize);
int pd_vk_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t bsize);
int pd_vk_tile(struct PDTileRequest *r, uint8_t *base, uint32_t bsize);

// why the last job fell back
const char *pd_vk_reason(void);

// microseconds the last job spent in each part
struct pd_vk_stats {
  uint64_t translate, pipeline, upload, run, readback;
  int translated_cached, pipeline_cached;
  // submissions and their waits for the queue, memory allocations, bytes up and back, freeing the job's objects
  uint64_t wait, up_bytes, down_bytes, teardown;
  int submits, allocs;
  // textures found unchanged in the per-job texture cache
  int tex_hits;
};

extern struct pd_vk_stats pd_vk_last;

// sharing the executor's device with a presenter (a window's swapchain). Extensions it needs are
// asked for before the executor starts, handles are the Vulkan ones, untyped to keep vulkan.h out
void pd_vk_want_extensions(const char *const *instance, unsigned ninstance, const char *const *device, unsigned ndevice);

struct pd_vk_handles {
  void *gipa, *instance, *physical, *device, *queue;
  uint32_t queue_family;
};

// starts the executor whether or not AIRJITD_VULKAN is set: 0 and the handles, or -1
int pd_vk_start(struct pd_vk_handles *h);

// resident mode, for clients that report CPU writes (pdmtl): resources stay on the device and jobs batch until
// pd_vk_flush, textures re-upload after pd_vk_dirty, buffers when changed, pd_vk_sync brings GPU writes back
void pd_vk_set_resident(int on, uint8_t *base, uint32_t bsize);
int pd_vk_resident(void);
void pd_vk_dirty(uint32_t off, uint32_t len);
void pd_vk_sync(uint32_t off, uint32_t len);
void pd_vk_sync_all(void);
// submits the batch (wait: until it ran), finish also brings back buffers the GPU wrote, as apps read those
void pd_vk_flush(int wait);
void pd_vk_finish(void);
// a resident attachment cleared on the GPU (which: 1 depth or colour, 2 stencil), 0 when not resident
int pd_vk_clear(const struct PDDrawTex *t, const float *value, uint32_t stencil, int which);

// an image the caller owns stands in for any draw attachment at bridge offset off: drawn in place
// (layout GENERAL before and after), never uploaded or read back. image 0 removes the binding
void pd_vk_external_attachment(uint32_t off, uint64_t image, uint64_t view);
// nonzero once after the CPU wrote the lent image's bytes since a draw last rendered into it
int pd_vk_external_written(void);

#endif
