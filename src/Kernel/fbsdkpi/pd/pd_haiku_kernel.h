// SPDX-License-Identifier: MIT
// private glue between the haiku api emulation and the puredarwin attach path
#ifndef PD_HAIKU_KERNEL_H
#define PD_HAIKU_KERNEL_H

#include <OS.h>
#include <PCI.h>
#include <arch_cpu_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void pd_cpu_pause(void) { SPINLOCK_PAUSE(); }

void thread_exit_with(status_t result) __attribute__((noreturn));

// the pci bus manager over registered iokit devices
pci_module_info *pd_pci_module(void);
// registers an IOPCIDevice and fills a haiku pci_info for it
status_t pd_pci_register(void *dev, pci_info *info);
void pd_pci_unregister(void *dev);
status_t pd_pci_add_peer(void *dev, pci_info *info);
void *pd_pci_iokit_device(uint8 bus, uint8 device, uint8 function);

#ifdef __cplusplus
}
#endif

#endif
