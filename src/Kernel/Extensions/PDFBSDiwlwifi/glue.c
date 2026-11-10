/*
 * Copyright 2026, PureDarwin Project. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
// iwlwifi registers its pci driver through linuxkpi at sysinit time

#include <sys/bus.h>

void pd_lkpi_init_hardware(void);

HAIKU_FBSD_WLAN_DRIVERS_GLUE(iwlwifi);
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

void
__haiku_init_hardware()
{
	pd_lkpi_init_hardware();
}
