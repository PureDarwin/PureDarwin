// SPDX-License-Identifier: MIT
// FreeBSD's dev/pci/pcivar.h with the additions linuxkpi expects
#pragma once
#include_next <dev/pci/pcivar.h>

// pcivar additions linuxkpi expects, implemented in pd_lkpi_pci.c
#define PCIV_INVALID 0xffff
extern int pci_enable_aspm;
bool is_pci_device(device_t dev);
device_t pci_find_bsf(uint8_t bus, uint8_t slot, uint8_t func);
device_t pci_find_device(uint16_t vendor, uint16_t device);
device_t pci_find_class_from(uint8_t class, uint8_t subclass, device_t devfrom);
device_t pci_find_base_class_from(uint8_t class, device_t devfrom);
device_t pci_find_pcie_root_port(device_t dev);
int pci_get_max_payload(device_t dev);
void pci_restore_state(device_t dev);
void pci_save_state(device_t dev);
bool pci_has_pme(device_t dev, int state);
int pci_disable_busmaster(device_t dev);
int pci_disable_io(device_t dev, int space);
uint8_t pci_get_class(device_t dev);
uint8_t pci_get_subclass(device_t dev);
void *vga_pci_map_bios(device_t dev, size_t *size);
void vga_pci_unmap_bios(device_t dev, void *bios);
