// SPDX-License-Identifier: MIT
// tegra210 wdt0 keep-alive for a watchdog the loader armed

#ifndef _PD_TEGRA_WATCHDOG_H
#define _PD_TEGRA_WATCHDOG_H

// pets wdt0 from a timer when the imported device tree has a tegra wdt and it is armed
void PDTegraWatchdog_startFromDeviceTree(void);

#endif
