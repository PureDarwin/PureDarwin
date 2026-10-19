/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Hugo Santos, hugosantos@gmail.com
 */


#include "device.h"

#include <compat/sys/bus.h>

#include <compat/net/if_media.h>
#include <compat/dev/mii/miivar.h>


// This symbol needs to be visible so that device_add_child can find it.
__attribute__ ((visibility ("default")))
driver_t *DRIVER_MODULE_NAME(miibus, driver) = &miibus_driver;


int
__haiku_miibus_readreg(device_t device, int phy, int reg)
{
	if (device->methods.miibus_readreg == NULL)
		panic("miibus_readreg, no support");

	return device->methods.miibus_readreg(device, phy, reg);
}


int
__haiku_miibus_writereg(device_t device, int phy, int reg, int data)
{
	if (device->methods.miibus_writereg == NULL)
		panic("miibus_writereg, no support");

	return device->methods.miibus_writereg(device, phy, reg, data);
}


void
__haiku_miibus_statchg(device_t device)
{
	if (device->methods.miibus_statchg)
		device->methods.miibus_statchg(device);
}


void
__haiku_miibus_linkchg(device_t device)
{
	if (device->methods.miibus_linkchg)
		device->methods.miibus_linkchg(device);
}


void
__haiku_miibus_mediainit(device_t device)
{
	if (device->methods.miibus_mediainit)
		device->methods.miibus_mediainit(device);
}
