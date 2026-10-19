/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <sys/cdefs.h>
#include <util/list.h>
#include <sys/haiku-module.h>
#include <device_if.h>

#include "../freebsd_network/shared.h"


void*
DEVICE_REGISTER(device_t dev)
{
	return dev->methods.device_register(dev);
}
