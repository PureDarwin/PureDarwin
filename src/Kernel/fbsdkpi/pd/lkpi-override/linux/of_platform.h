// SPDX-License-Identifier: MIT
// of_platform: platform devices are created by the iokit attach path, not from here
#ifndef _LINUXKPI_LINUX_OF_PLATFORM_H
#define	_LINUXKPI_LINUX_OF_PLATFORM_H

#include <linux/of_device.h>
#include <linux/platform_device.h>

struct platform_device *of_find_device_by_node(struct device_node *);

#endif
