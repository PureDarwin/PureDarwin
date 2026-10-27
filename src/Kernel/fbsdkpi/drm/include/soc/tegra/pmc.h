// SPDX-License-Identifier: MIT
// tegra power-gate control, FreeBSD's tegra210 pmc code behind it on arm64
#ifndef _PD_SOC_TEGRA_PMC_H_
#define _PD_SOC_TEGRA_PMC_H_

#include <linux/errno.h>

// the partition ids of freebsd's tegra_pmc.h, where 32 names the gpu rail clamp
#define TEGRA_POWERGATE_3D 32

#ifdef CONFIG_ARM64
int tegra_powergate_remove_clamping(unsigned int id);
#else
static inline int tegra_powergate_remove_clamping(unsigned int id) { return -ENODEV; }
#endif

#endif
