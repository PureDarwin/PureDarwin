/*
 * Copyright 2026, PureDarwin Project. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
// wcn36xx registers its platform driver through linuxkpi at sysinit time

#include <sys/bus.h>

void pd_lkpi_init_hardware(void);

HAIKU_FBSD_WLAN_DRIVERS_GLUE(wcn36xx);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES | FBSD_WLAN);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// linuxkpi handlers run on haiku's interrupt thread, every interrupt is passed
// there, so this only works for message interrupts that cannot be shared
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 1;
}

// the wcnss dxe reads and writes ram without snooping the cpu caches, dma here needs cache upkeep
extern int pd_dma_noncoherent;
// pronto drops protected frames it holds no key for, so keys go to the firmware as in linux
extern bool lkpi_hwcrypto;

void
__haiku_init_hardware()
{
	pd_dma_noncoherent = 1;
	lkpi_hwcrypto = true;
	pd_lkpi_init_hardware();
}
