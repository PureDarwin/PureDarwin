/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <sys/bus.h>


extern driver_t* DRIVER_MODULE_NAME(em, pci);
extern driver_t* DRIVER_MODULE_NAME(igb, pci);

HAIKU_FBSD_DRIVERS_GLUE(ipro1000);
HAIKU_DRIVER_REQUIREMENTS(0);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_CHECK_DISABLE_INTERRUPTS();
NO_HAIKU_REENABLE_INTERRUPTS();


void
__haiku_init_hardware()
{
	driver_t *drivers[] = {
		DRIVER_MODULE_NAME(em, pci),
		DRIVER_MODULE_NAME(igb, pci),
		NULL
	};
	_fbsd_init_hardware_pci(drivers);
}
