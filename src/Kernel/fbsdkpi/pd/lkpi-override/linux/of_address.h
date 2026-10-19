// SPDX-License-Identifier: MIT
// register windows of device tree nodes, translated to cpu addresses (pd_linux_of.c)
#ifndef _LINUXKPI_LINUX_OF_ADDRESS_H
#define	_LINUXKPI_LINUX_OF_ADDRESS_H

#include <linux/of.h>
#include <linux/ioport.h>

int of_address_to_resource(struct device_node *, int, struct resource *);
void __iomem *of_iomap(struct device_node *, int);

#endif
