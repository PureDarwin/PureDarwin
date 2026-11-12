// SPDX-License-Identifier: MIT
// pd_fbsd_glue.c, the freebsd pci calls every fbsdkpi driver links against
#ifndef PD_FBSD_GLUE_H
#define PD_FBSD_GLUE_H

#include <stdbool.h>
#include <stdint.h>

int pci_disable_busmaster(device_t dev);
int pci_disable_io(device_t dev, int space);
int pci_get_max_payload(device_t dev);
void pci_save_state(device_t dev);
void pci_restore_state(device_t dev);
bool pci_has_pme(device_t dev, int state);
void pci_clear_pme(device_t dev);
void pci_enable_pme(device_t dev);
uint8_t pci_get_class(device_t dev);
uint8_t pci_get_subclass(device_t dev);

#endif
