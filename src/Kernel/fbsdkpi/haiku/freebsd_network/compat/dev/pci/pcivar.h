/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_DEV_PCI_PCIVAR_H_
#define _FBSD_COMPAT_DEV_PCI_PCIVAR_H_


#include <sys/bus.h>


#define	PCI_RF_DENSE	0x10000
	// ignored on x86

#define	PCI_POWERSTATE_D0	0
#define	PCI_POWERSTATE_D1	1
#define	PCI_POWERSTATE_D2	2
#define	PCI_POWERSTATE_D3	3
#define	PCI_POWERSTATE_UNKNOWN	-1


int pci_enable_busmaster(device_t dev);
int pci_enable_io(device_t dev, int reg);

uint32_t pci_get_devid(device_t dev);
void pci_set_intpin(device_t dev, uint8_t pin);
uint8_t pci_get_intpin(device_t dev);

uint16_t pci_get_vendor(device_t dev);
uint16_t pci_get_device(device_t dev);
uint16_t pci_get_subvendor(device_t dev);
uint16_t pci_get_subdevice(device_t dev);
uint8_t pci_get_revid(device_t dev);
uint8_t pci_get_cachelnsz(device_t dev);
uint8_t *pci_get_ether(device_t dev);

uint32_t pci_read_config(device_t dev, int reg, int width);
void pci_write_config(device_t dev, int reg, uint32_t val, int width);

uint32_t pci_get_domain(device_t dev);
uint8_t pci_get_bus(device_t dev);
uint8_t pci_get_slot(device_t dev);
uint8_t pci_get_function(device_t dev);
device_t pci_find_dbsf(uint32_t domain, uint8_t bus, uint8_t slot,
	uint8_t func);

int pci_find_cap(device_t dev, int capability, int *capreg);
int pci_find_extcap(device_t dev, int capability, int *capreg);

int pci_msi_count(device_t dev);
int pci_alloc_msi(device_t dev, int *count);
int pci_release_msi(device_t dev);

int pci_msix_table_bar(device_t dev);
int pci_msix_count(device_t dev);
int pci_alloc_msix(device_t dev, int *count);

int pci_get_max_read_req(device_t dev);
int pci_set_max_read_req(device_t dev, int size);

int pci_get_powerstate(device_t dev);
int pci_set_powerstate(device_t dev, int newPowerState);

static inline int
pci_get_vpd_ident(device_t dev, const char **identptr)
{
	return -1;
}

#endif	/* _FBSD_COMPAT_DEV_PCI_PCIVAR_H_ */
