#ifndef _PD_SUN50I_WATCHDOG_H
#define _PD_SUN50I_WATCHDOG_H

// arms the h616 watchdog when the pdwdt boot-arg asks for it
void PDSun50iWatchdog_start(void);
// the same for a board whose imported device tree names an allwinner wdt-v103 (a733)
void PDSun50iWatchdog_startFromDeviceTree(void);

#endif
