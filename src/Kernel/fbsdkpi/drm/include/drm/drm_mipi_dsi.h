// SPDX-License-Identifier: MIT
// the mipi dsi types i915's non-dsi display code names

#ifndef _PD_DRM_MIPI_DSI_H_
#define _PD_DRM_MIPI_DSI_H_

#include <linux/device.h>
#include <linux/list.h>
#include <linux/types.h>

struct drm_dsc_picture_parameter_set;
struct mipi_dsi_host;
struct mipi_dsi_device;

// pixel formats a DSI link carries, in the order the VBT encodes them
enum mipi_dsi_pixel_format {
	MIPI_DSI_FMT_RGB888,
	MIPI_DSI_FMT_RGB666,
	MIPI_DSI_FMT_RGB666_PACKED,
	MIPI_DSI_FMT_RGB565,
};

#define MIPI_DSI_MODE_LPM		(1UL << 11)
#define MIPI_DSI_MSG_USE_LPM		(1U << 1)

struct mipi_dsi_msg {
	u8 channel;
	u8 type;
	u16 flags;
	size_t tx_len;
	const void *tx_buf;
	size_t rx_len;
	void *rx_buf;
};

struct mipi_dsi_packet {
	size_t size;
	u8 header[4];
	size_t payload_length;
	const u8 *payload;
};

struct mipi_dsi_host_ops {
	int (*attach)(struct mipi_dsi_host *host, struct mipi_dsi_device *dsi);
	int (*detach)(struct mipi_dsi_host *host, struct mipi_dsi_device *dsi);
	ssize_t (*transfer)(struct mipi_dsi_host *host,
	    const struct mipi_dsi_msg *msg);
};

struct mipi_dsi_host {
	struct device *dev;
	const struct mipi_dsi_host_ops *ops;
	struct list_head list;
};

struct mipi_dsi_device {
	struct mipi_dsi_host *host;
	struct device dev;
	char name[20];
	unsigned int channel;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	unsigned long hs_rate;
	unsigned long lp_rate;
};

static inline int
mipi_dsi_pixel_format_to_bpp(enum mipi_dsi_pixel_format fmt)
{
	switch (fmt) {
	case MIPI_DSI_FMT_RGB888:
	case MIPI_DSI_FMT_RGB666:
		return 24;
	case MIPI_DSI_FMT_RGB666_PACKED:
		return 18;
	case MIPI_DSI_FMT_RGB565:
		return 16;
	}
	return -EINVAL;
}

ssize_t mipi_dsi_picture_parameter_set(struct mipi_dsi_device *dsi,
    const struct drm_dsc_picture_parameter_set *pps);
ssize_t mipi_dsi_compression_mode(struct mipi_dsi_device *dsi, bool enable);

#endif
