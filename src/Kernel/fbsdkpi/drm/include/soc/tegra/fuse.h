// SPDX-License-Identifier: MIT
// tegra fuse readout: the sku fields nouveau reads, filled from the efuse on tegra210
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
