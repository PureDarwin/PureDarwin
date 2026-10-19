// SPDX-License-Identifier: MIT
// platform devices from the device tree, replacing linuxkpi's stub (pd_linux_platform.c)
#ifndef _LINUXKPI_LINUX_PLATFORM_DEVICE_H
#define	_LINUXKPI_LINUX_PLATFORM_DEVICE_H

#include <sys/param.h>
#include <sys/bus.h>
#include <machine/resource.h>

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/mod_devicetable.h>

#ifndef IORESOURCE_MEM
#define	IORESOURCE_MEM	(1 << SYS_RES_MEMORY)
#define	IORESOURCE_IO	(1 << SYS_RES_IOPORT)
#define	IORESOURCE_IRQ	(1 << SYS_RES_IRQ)
#endif

struct platform_device_id {
	char		name[20];
	unsigned long	driver_data;
};

struct platform_device {
	const char			*name;
	int				id;
	bool				id_auto;
	struct device			dev;
	u32				num_resources;
	struct resource			*resource;
	const struct platform_device_id	*id_entry;
	// the iokit nub interrupts are installed on, and the node's interrupt count
	void				*pd_nub;
	int				pd_nirq;
};

struct platform_driver {
	int	(*probe)(struct platform_device *);
	void	(*remove)(struct platform_device *);
	void	(*shutdown)(struct platform_device *);
	struct device_driver	driver;
	const struct platform_device_id *id_table;
	// the probe that matched, kept for remove and shutdown at teardown
	struct list_head	pd_link;
};

#define	to_platform_device(x)	container_of((x), struct platform_device, dev)

// true for a struct device this platform bus created
bool pd_dev_is_platform(const struct device *);
#define	dev_is_platform(dev)	pd_dev_is_platform(dev)

// devices come from the device tree only, registering one by hand does nothing
static inline int
platform_device_register(struct platform_device *pdev)
{
	return (0);
}

static inline void
platform_device_unregister(struct platform_device *pdev)
{
}

int platform_driver_register(struct platform_driver *);
void platform_driver_unregister(struct platform_driver *);
int platform_register_drivers(struct platform_driver * const *, unsigned int);
void platform_unregister_drivers(struct platform_driver * const *, unsigned int);

#define	module_platform_driver(_drv)					\
static int __init _drv##_init(void) { return (platform_driver_register(&(_drv))); } \
static void __exit _drv##_exit(void) { platform_driver_unregister(&(_drv)); } \
module_init(_drv##_init);						\
module_exit(_drv##_exit)

struct resource *platform_get_resource(struct platform_device *, unsigned int, unsigned int);
struct resource *platform_get_resource_byname(struct platform_device *, unsigned int, const char *);
int platform_get_irq(struct platform_device *, unsigned int);
int platform_get_irq_optional(struct platform_device *, unsigned int);
int platform_get_irq_byname(struct platform_device *, const char *);
int platform_irq_count(struct platform_device *);
void __iomem *devm_platform_ioremap_resource(struct platform_device *, unsigned int);
void __iomem *devm_platform_ioremap_resource_byname(struct platform_device *, const char *);
void __iomem *devm_platform_get_and_ioremap_resource(struct platform_device *, unsigned int,
    struct resource **);
void __iomem *devm_ioremap_resource(struct device *, const struct resource *);

static inline void *
platform_get_drvdata(const struct platform_device *pdev)
{
	return (dev_get_drvdata(&pdev->dev));
}

static inline void
platform_set_drvdata(struct platform_device *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

static inline void *
dev_get_platdata(struct device *dev)
{
	return (NULL);
}

#endif
