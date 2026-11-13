// pdnv_pci.h - between the IOKit side of a GPU and the ported interface layer
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PDNV_PCI_H
#define PDNV_PCI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// published by the loader once rm_init_rm has returned true
#define PDNV_RM_RESOURCE    "PDNVResourceManager"

#define PDNV_MAX_BARS       3
#define PDNV_MAP_CACHED     0
#define PDNV_MAP_UNCACHED   1
#define PDNV_MAP_WC         2

struct pdnv_pci_bar {
	uint64_t cpu_address;
	uint64_t size;
	uint32_t offset;            // the BAR's config space register
};

struct pdnv_pci_dev {
	void *handle;               // the IOPCIDevice
	uint16_t vendor, device, subsystem_vendor, subsystem_id;
	uint8_t bus, slot, func;
	uint32_t irq;
	int nbars;
	struct pdnv_pci_bar bars[PDNV_MAX_BARS];
};

// pdnv_pcidev.cpp
void pdnv_pci_init(void);
int pdnv_cfg_read(void *handle, uint32_t off, int width, uint32_t *v);
int pdnv_cfg_write(void *handle, uint32_t off, int width, uint32_t v);
void *pdnv_pci_find(uint32_t domain, uint8_t bus, uint8_t slot, uint8_t func, uint16_t *vendor, uint16_t *device);
void *pdnv_map_phys(uint64_t pa, uint64_t size, int cache);
void pdnv_unmap_phys(void *va);
void *pdnv_dma_alloc(uint64_t size, int cache, uint64_t mask, uint64_t *pa, void **va);
void pdnv_dma_free(void *handle);
uint64_t pdnv_kvtophys(uint64_t va);      // 0 when nothing is mapped there
void *pdnv_map_runs(const uint64_t *pa, uint32_t n, uint64_t run, int cache, void **cookie);
void pdnv_unmap_runs(void *cookie);

// pdnv_pci.c: builds the device's nv_state_t and runs the resource manager's probe on it, NULL if refused
void *pdnv_gpu_probe(const struct pdnv_pci_dev *info);
int pdnv_gpu_isr(void *gpu);                // 0 not ours, 1 handled, 2 handled and the bottom half is due
void pdnv_gpu_bh(void *gpu);
int pdnv_gpu_start(void *gpu);
void *pdnv_gpu_record(void *nv);            // nv_state_t *: an os_state for the control device
void *pdnv_gpu_state(void *gpu);            // its nv_state_t
void pdnv_work_init(void);

// system memory allocations, for the user mappings pdnv_dev.c makes
void pdnv_alloc_retain(void *at);
void pdnv_alloc_release(void *at);
uint64_t pdnv_alloc_pages(void *at);
uint64_t pdnv_alloc_page_pa(void *at, uint64_t page);
uint32_t pdnv_alloc_cache(void *at);                  // the deferred work thread, once the resource manager is up

// pdnv_dev.c: /dev/nvidiactl once the resource manager is up, /dev/nvidiaN for each started GPU
int pdnv_dev_init(void);
int pdnv_dev_add_gpu(void *nv);

// where the object is linked, for offsets in logs that would otherwise be redacted
extern uintptr_t pdnv_image_base;

#ifdef __cplusplus
}
#endif

#endif
