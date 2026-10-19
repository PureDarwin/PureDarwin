// SPDX-License-Identifier: MIT
// match data and helpers for device tree platform devices (pd_linux_of.c)
#ifndef _LINUXKPI_LINUX_OF_DEVICE_H
#define	_LINUXKPI_LINUX_OF_DEVICE_H

#include <linux/of.h>
#include <linux/device.h>

const struct of_device_id *of_match_device(const struct of_device_id *, const struct device *);
const void *of_device_get_match_data(const struct device *);
#define	device_get_match_data(dev)	of_device_get_match_data(dev)

#endif
