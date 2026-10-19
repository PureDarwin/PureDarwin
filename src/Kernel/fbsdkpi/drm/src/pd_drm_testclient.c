// SPDX-License-Identifier: MIT
// a small in-kernel drm client for bring-up
// colour bars on every lit crtc, so a modeset can be checked on a monitor
#include <linux/iosys-map.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <drm/drm_client.h>
#include <drm/drm_device.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modes.h>
#include <drm/drm_print.h>

int pd_drm_test_client(struct drm_device *dev);

static const struct drm_client_funcs pd_test_client_funcs = {
	.owner = THIS_MODULE,
};

static void
pd_test_fill(struct iosys_map *map, u32 width, u32 height, u32 pitch)
{
	static const u32 bars[] = { 0x00ff0000, 0x0000ff00, 0x000000ff,
	    0x00ffffff, 0x00ffff00, 0x0000ffff, 0x00ff00ff, 0x00000000 };
	u32 x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			u32 c = bars[(x * 8) / width];
			iosys_map_wr(map, (size_t)y * pitch + x * 4, u32, c);
		}
	}
}

int
pd_drm_test_client(struct drm_device *dev)
{
	struct drm_client_dev *client;
	struct drm_client_buffer *buf;
	struct drm_mode_set *modeset;
	struct iosys_map map;
	u32 width = 0, height = 0;
	int ret;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL)
		return -ENOMEM;
	ret = drm_client_init(dev, client, "pdtest", &pd_test_client_funcs);
	if (ret != 0) {
		kfree(client);
		return ret;
	}
	ret = drm_client_modeset_probe(client, 0, 0);
	if (ret != 0) {
		drm_err(dev, "pdtest: modeset probe failed %d\n", ret);
		return ret;
	}

	drm_client_for_each_modeset(modeset, client) {
		if (modeset->mode == NULL)
			continue;
		drm_info(dev, "pdtest: crtc %d mode %dx%d@%d on %d connector(s)\n",
		    modeset->crtc->base.id, modeset->mode->hdisplay,
		    modeset->mode->vdisplay, drm_mode_vrefresh(modeset->mode),
		    modeset->num_connectors);
		width = max_t(u32, width, modeset->mode->hdisplay);
		height = max_t(u32, height, modeset->mode->vdisplay);
	}
	if (width == 0) {
		drm_info(dev, "pdtest: no connected display\n");
		return -ENODEV;
	}

	buf = drm_client_framebuffer_create(client, width, height, DRM_FORMAT_XRGB8888);
	if (IS_ERR(buf)) {
		drm_err(dev, "pdtest: framebuffer %ux%u failed %ld\n", width, height, PTR_ERR(buf));
		return PTR_ERR(buf);
	}
	ret = drm_client_buffer_vmap(buf, &map);
	if (ret != 0) {
		drm_err(dev, "pdtest: vmap failed %d\n", ret);
		return ret;
	}
	pd_test_fill(&map, width, height, buf->fb->pitches[0]);

	drm_client_for_each_modeset(modeset, client) {
		if (modeset->mode != NULL)
			modeset->fb = buf->fb;
	}
	ret = drm_client_modeset_commit(client);
	drm_info(dev, "pdtest: commit %ux%u returned %d\n", width, height, ret);
	return ret;
}
