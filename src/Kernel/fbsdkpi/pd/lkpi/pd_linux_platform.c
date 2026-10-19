// SPDX-License-Identifier: MIT
// the platform bus for device tree drivers: registry, matching, probe and resources

#include <sys/param.h>
#include <sys/bus.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pd_providers.h>

#include "pd_fbsd_os.h"
#include "pd_fbsd_attach.h"

struct device_node *pd_of_node_for_entry(void *entry);
int pd_linux_dev_dma_init(struct device *dev, u64 mask);
// a vector range in the haiku interrupt table for a device tree nub (pd_pci.c)
int pd_platform_nub_vectors(void *nub);
// a newbus device for the struct device's bsddev, under one shared root (pd_pci.c)
device_t pd_platform_bsddev(const char *name, const char *desc);

static LIST_HEAD(sDrivers);
// set up on first use, which is single threaded (module init or the first attach)
static struct mutex pd_platform_lock;
static int pd_platform_lock_ready;

static struct mutex *
pd_platform_lock_get(void)
{
	if (!pd_platform_lock_ready) {
		mutex_init(&pd_platform_lock);
		pd_platform_lock_ready = 1;
	}
	return (&pd_platform_lock);
}
static struct platform_device *sDevices[16];

int
platform_driver_register(struct platform_driver *pdrv)
{
	mutex_lock(pd_platform_lock_get());
	list_add_tail(&pdrv->pd_link, &sDrivers);
	mutex_unlock(pd_platform_lock_get());
	return (0);
}

void
platform_driver_unregister(struct platform_driver *pdrv)
{
	mutex_lock(pd_platform_lock_get());
	list_del(&pdrv->pd_link);
	mutex_unlock(pd_platform_lock_get());
}

int
platform_register_drivers(struct platform_driver * const *drivers, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++)
		platform_driver_register(drivers[i]);
	return (0);
}

void
platform_unregister_drivers(struct platform_driver * const *drivers, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++)
		platform_driver_unregister(drivers[i]);
}

struct platform_device *
of_find_device_by_node(struct device_node *np)
{
	for (int i = 0; i < nitems(sDevices); i++) {
		if (sDevices[i] != NULL && sDevices[i]->dev.of_node == np)
			return (sDevices[i]);
	}
	return (NULL);
}

static void
platform_release(struct device *dev)
{
}

bool
pd_dev_is_platform(const struct device *dev)
{
	return (dev != NULL && dev->release == platform_release);
}

void *
pd_lkpi_platform_attach(const struct pd_fbsd_platform *pd)
{
	struct device_node *np = pd_of_node_for_entry(pd->node);
	struct platform_driver *pdrv = NULL, *it;
	struct platform_device *pdev;
	int score = 0, error, vectors, slot = -1;

	if (np == NULL)
		return (NULL);
	mutex_lock(pd_platform_lock_get());
	list_for_each_entry(it, &sDrivers, pd_link) {
		const struct of_device_id *m = of_match_node(it->driver.of_match_table, np);
		int s = m != NULL ? of_device_is_compatible(np, m->compatible) : 0;
		if (s > score) {
			score = s;
			pdrv = it;
		}
	}
	mutex_unlock(pd_platform_lock_get());
	if (pdrv == NULL) {
		printf("pd platform: no driver for %s\n", np->full_name);
		return (NULL);
	}
	for (int i = 0; i < nitems(sDevices); i++) {
		if (sDevices[i] == NULL) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return (NULL);

	pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
	pdev->name = np->name;
	pdev->id = -1;
	pdev->pd_nub = pd->nub;
	pdev->pd_nirq = pd->nirq;
	pdev->num_resources = pd->nmem + pd->nirq;
	pdev->resource = kcalloc(pdev->num_resources + 1, sizeof(struct resource), GFP_KERNEL);
	for (int i = 0; i < pd->nmem; i++) {
		struct resource *r = &pdev->resource[i];
		const char *name = NULL;
		of_property_read_string_index(np, "reg-names", i, &name);
		r->start = pd->mem_phys[i];
		r->end = pd->mem_phys[i] + pd->mem_size[i] - 1;
		r->flags = IORESOURCE_MEM;
		r->name = name;
	}
	vectors = pd->nirq != 0 ? pd_platform_nub_vectors(pd->nub) : -1;
	for (int i = 0; i < pd->nirq; i++) {
		struct resource *r = &pdev->resource[pd->nmem + i];
		const char *name = NULL;
		of_property_read_string_index(np, "interrupt-names", i, &name);
		r->start = r->end = vectors >= 0 ? vectors + i : -1;
		r->flags = IORESOURCE_IRQ;
		r->name = name;
	}

	struct device *dev = &pdev->dev;
	kobject_init(&dev->kobj, &linux_dev_ktype);
	kobject_set_name(&dev->kobj, "%s", np->full_name);
	spin_lock_init(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
	INIT_LIST_HEAD(&dev->irqents);
	dev->parent = &linux_root_device;
	dev->of_node = np;
	dev->driver = &pdrv->driver;
	dev->release = platform_release;
	dev->bsddev = pd_platform_bsddev(np->name, np->full_name);
	pd_linux_dev_dma_init(dev, DMA_BIT_MASK(32));

	pd_power_domains_on(np);
	linux_set_current(curthread);
	printf("pd platform: probing %s with %s\n", np->full_name,
	    pdrv->driver.name != NULL ? pdrv->driver.name : "?");
	error = pdrv->probe(pdev);
	if (error != 0) {
		printf("pd platform: %s probe returned %d\n", np->full_name, error);
		lkpi_devres_release_free_list(dev);
		return (NULL);
	}
	sDevices[slot] = pdev;
	return (dev);
}

struct resource *
platform_get_resource(struct platform_device *pdev, unsigned int type, unsigned int num)
{
	for (u32 i = 0; i < pdev->num_resources; i++) {
		struct resource *r = &pdev->resource[i];
		if ((r->flags & type) != 0 && num-- == 0)
			return (r);
	}
	return (NULL);
}

struct resource *
platform_get_resource_byname(struct platform_device *pdev, unsigned int type, const char *name)
{
	for (u32 i = 0; i < pdev->num_resources; i++) {
		struct resource *r = &pdev->resource[i];
		if ((r->flags & type) != 0 && r->name != NULL && strcmp(r->name, name) == 0)
			return (r);
	}
	return (NULL);
}

int
platform_get_irq_optional(struct platform_device *pdev, unsigned int num)
{
	struct resource *r = platform_get_resource(pdev, IORESOURCE_IRQ, num);

	return (r != NULL && (long)r->start >= 0 ? (int)r->start : -ENXIO);
}

int
platform_get_irq(struct platform_device *pdev, unsigned int num)
{
	int irq = platform_get_irq_optional(pdev, num);

	if (irq < 0)
		dev_err(&pdev->dev, "no interrupt %u\n", num);
	return (irq);
}

int
platform_get_irq_byname(struct platform_device *pdev, const char *name)
{
	struct resource *r = platform_get_resource_byname(pdev, IORESOURCE_IRQ, name);

	return (r != NULL && (long)r->start >= 0 ? (int)r->start : -ENXIO);
}

int
platform_irq_count(struct platform_device *pdev)
{
	return (pdev->pd_nirq);
}

struct pd_iomap {
	void __iomem	*va;
};

static void
devm_iounmap_action(void *arg)
{
	struct pd_iomap *m = arg;

	iounmap(m->va);
	kfree(m);
}

void __iomem *
devm_ioremap_resource(struct device *dev, const struct resource *res)
{
	struct pd_iomap *m;

	if (res == NULL || (res->flags & IORESOURCE_MEM) == 0)
		return ((void __iomem *)ERR_PTR(-EINVAL));
	m = kzalloc(sizeof(*m), GFP_KERNEL);
	m->va = ioremap(res->start, resource_size(res));
	if (m->va == NULL) {
		kfree(m);
		return ((void __iomem *)ERR_PTR(-ENOMEM));
	}
	lkpi_devm_add_action(dev, devm_iounmap_action, m);
	return (m->va);
}

void __iomem *
devm_platform_get_and_ioremap_resource(struct platform_device *pdev, unsigned int index,
    struct resource **out)
{
	struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, index);

	if (out != NULL)
		*out = r;
	return (devm_ioremap_resource(&pdev->dev, r));
}

void __iomem *
devm_platform_ioremap_resource(struct platform_device *pdev, unsigned int index)
{
	return (devm_platform_get_and_ioremap_resource(pdev, index, NULL));
}

void __iomem *
devm_platform_ioremap_resource_byname(struct platform_device *pdev, const char *name)
{
	return (devm_ioremap_resource(&pdev->dev,
	    platform_get_resource_byname(pdev, IORESOURCE_MEM, name)));
}
