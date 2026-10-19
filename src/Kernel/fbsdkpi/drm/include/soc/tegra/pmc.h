// SPDX-License-Identifier: MIT
// tegra power-gate control, filled in once PD runs on a tegra soc
#ifndef _PD_SOC_TEGRA_PMC_H_
#define _PD_SOC_TEGRA_PMC_H_

#include <linux/errno.h>

#define TEGRA_POWERGATE_3D 1

static inline int tegra_powergate_remove_clamping(unsigned int id) { return -ENODEV; }

#endif
