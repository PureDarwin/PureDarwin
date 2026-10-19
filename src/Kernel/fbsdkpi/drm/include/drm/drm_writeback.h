// SPDX-License-Identifier: MIT
// drm writeback types, the fields drm core and amdgpu_dm touch
// creating a writeback connector fails, so no job ever reaches these helpers
#ifndef _PD_DRM_WRITEBACK_H_
#define _PD_DRM_WRITEBACK_H_

#include <linux/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>

struct drm_framebuffer;
struct dma_fence;

struct drm_writeback_connector {
	struct drm_connector base;
	struct drm_encoder encoder;
	struct drm_property_blob *pixel_formats_blob_ptr;
	spinlock_t job_lock;
	struct list_head job_queue;
	unsigned int fence_context;
	spinlock_t fence_lock;
	unsigned long fence_seqno;
	char timeline_name[32];
};

struct drm_writeback_job {
	struct drm_writeback_connector *connector;
	bool prepared;
	struct work_struct cleanup_work;
	struct list_head list_entry;
	struct drm_framebuffer *fb;
	struct dma_fence *out_fence;
	void *priv;
};

static inline struct drm_writeback_connector *
drm_connector_to_writeback(struct drm_connector *connector)
{
	return container_of(connector, struct drm_writeback_connector, base);
}

static inline int
drm_writeback_connector_init(struct drm_device *dev,
    struct drm_writeback_connector *wb_connector,
    const struct drm_connector_funcs *con_funcs,
    const struct drm_encoder_helper_funcs *enc_helper_funcs,
    const u32 *formats, int n_formats, u32 possible_crtcs)
{
	return -ENODEV;
}

static inline int
drm_writeback_set_fb(struct drm_connector_state *conn_state,
    struct drm_framebuffer *fb)
{
	return fb != NULL ? -EINVAL : 0;
}

static inline int
drm_writeback_prepare_job(struct drm_writeback_job *job)
{
	return 0;
}

static inline void
drm_writeback_queue_job(struct drm_writeback_connector *wb_connector,
    struct drm_connector_state *conn_state)
{
}

static inline void
drm_writeback_cleanup_job(struct drm_writeback_job *job)
{
}

static inline void
drm_writeback_signal_completion(struct drm_writeback_connector *wb_connector,
    int status)
{
}

static inline struct dma_fence *
drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector)
{
	return NULL;
}

#endif
