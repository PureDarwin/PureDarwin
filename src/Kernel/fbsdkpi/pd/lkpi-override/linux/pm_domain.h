// SPDX-License-Identifier: MIT
// power domains: the platform attach powers a node's domains before probe and keeps them on
#ifndef _LINUXKPI_LINUX_PM_DOMAIN_H
#define	_LINUXKPI_LINUX_PM_DOMAIN_H

#include <linux/device.h>

static inline int
dev_pm_domain_attach(struct device *dev, bool power_on)
{
	return (0);
}

static inline void
dev_pm_domain_detach(struct device *dev, bool power_off)
{
}

#endif
