// SPDX-License-Identifier: MIT
// FreeBSD's sys/bus.h with the newbus calls linuxkpi makes
#pragma once
#include_next <sys/bus.h>
#include <machine/bus.h>
#include <sys/bus_dma.h>

// newbus calls linux_pci.c makes for rescans and drm, answered by pd_lkpi_glue.c
devclass_t devclass_create(const char *classname);
int devclass_delete_driver(devclass_t busclass, driver_t *driver);
driver_t *device_get_driver(device_t dev);
int device_probe_and_attach(device_t dev);
void bus_attach_children(device_t dev);
void bus_identify_children(device_t dev);
int bus_get_domain(device_t dev, int *domain);
#define BUS_RESCAN(dev) ENODEV
bool bus_dma_id_mapped(bus_dma_tag_t dmat, vm_paddr_t buf, bus_size_t buflen);
int _bus_dmamap_load_phys(bus_dma_tag_t dmat, bus_dmamap_t map, vm_paddr_t buf,
    bus_size_t buflen, int flags, bus_dma_segment_t *segs, int *segp);
