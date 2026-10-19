// SPDX-License-Identifier: MIT
// tegra fuse readout, filled in once PD runs on a tegra soc
#ifndef _PD_SOC_TEGRA_FUSE_H_
#define _PD_SOC_TEGRA_FUSE_H_

#include <linux/types.h>
#include <linux/errno.h>

struct tegra_sku_info {
	int gpu_speedo_id;
	int gpu_speedo_value;
};

extern struct tegra_sku_info tegra_sku_info;

static inline int tegra_fuse_readl(unsigned long offset, u32 *value) { return -ENODEV; }

#endif
