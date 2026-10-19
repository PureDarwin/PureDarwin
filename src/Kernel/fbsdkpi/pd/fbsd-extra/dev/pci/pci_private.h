// SPDX-License-Identifier: MIT
// FreeBSD's pci bus internals that linuxkpi reaches into
#pragma once
#include <sys/bus.h>

// the pci bus internals linuxkpi reaches into, filled in by pd_lkpi_pci.c
// when a linux pci driver attaches
typedef uint64_t rman_res_t;

struct pci_map {
	uint64_t pm_value;
	uint64_t pm_size;
	uint16_t pm_reg;
};

struct resource_list_entry {
	int type;
	int rid;
	struct resource *res;
	rman_res_t start;
	rman_res_t end;
	rman_res_t count;
};

#define PD_RESOURCE_LIST_MAX 48

struct resource_list {
	int count;
	struct resource_list_entry entries[PD_RESOURCE_LIST_MAX];
};

struct pcicfg_msi {
	uint16_t msi_ctrl;
	uint8_t msi_location;
	uint8_t msi_msgnum;
	int msi_alloc;
	uint64_t msi_addr;
	uint16_t msi_data;
	u_int msi_handlers;
};

struct pcicfg_pcie {
	uint8_t pcie_location;
	uint8_t pcie_type;
	uint16_t pcie_flags;
};

struct pcicfg {
	struct pcicfg_msi msi;
	struct pcicfg_pcie pcie;
	uint16_t vendor;
	uint16_t device;
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
};

struct pci_devinfo {
	struct resource_list resources;
	struct pcicfg cfg;
	struct pci_map bars[6];
};

struct pci_map *pci_find_bar(device_t dev, int reg);
struct resource_list_entry *resource_list_find(struct resource_list *rl, int type, int rid);
struct resource *pci_reserve_map(device_t dev, device_t child, int type, int rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int num, u_int flags);
int bus_translate_resource(device_t dev, int type, rman_res_t start, rman_res_t *newstart);

#define PCI_ID_RID 0
int pd_lkpi_pci_get_rid(device_t dev, uintptr_t *rid);
#define PCI_GET_ID(parent, dev, type, idp) pd_lkpi_pci_get_rid(dev, idp)

struct driver;
int pd_lkpi_pci_driver_add(struct driver *driver);
void pd_lkpi_pci_msi_added(device_t dev, int count);
