/*
 * Copyright 2023, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <sys/bus.h>


extern driver_t* DRIVER_MODULE_NAME(rge, pci);

HAIKU_FBSD_DRIVERS_GLUE(rtl8125);
HAIKU_DRIVER_REQUIREMENTS(0);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();


// a stable locally administered address from the device name, for parts without one
void
pd_rge_gen_addr(const char *name, unsigned char *eaddr)
{
	uint32_t hash = 2166136261u;

	for (; *name != '\0'; name++)
		hash = (hash ^ (unsigned char)*name) * 16777619u;
	eaddr[0] = 0x02;
	eaddr[1] = 0x50;
	eaddr[2] = hash >> 24;
	eaddr[3] = hash >> 16;
	eaddr[4] = hash >> 8;
	eaddr[5] = hash;
}


int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	// rge only uses MSI, so every interrupt is ours
	return 1;
}


void
__haiku_init_hardware()
{
	driver_t *drivers[] = {
		DRIVER_MODULE_NAME(rge, pci),
		NULL
	};
	_fbsd_init_hardware_pci(drivers);
}
