// SPDX-License-Identifier: MIT
// freebsd pci calls past what the haiku layer offers, linked into every fbsdkpi driver

#include <compat/sys/bus.h>
#include <compat/sys/rman.h>
#include <compat/dev/pci/pcireg.h>
#include <compat/dev/pci/pcivar.h>

#include "pd_fbsd_glue.h"

int
pci_disable_busmaster(device_t dev)
{
	pci_write_config(dev, PCIR_COMMAND, pci_read_config(dev, PCIR_COMMAND, 2)
		& ~PCIM_CMD_BUSMASTEREN, 2);
	return 0;
}

int
pci_disable_io(device_t dev, int space)
{
	int bit = space == SYS_RES_IOPORT ? PCIM_CMD_PORTEN : PCIM_CMD_MEMEN;
	pci_write_config(dev, PCIR_COMMAND, pci_read_config(dev, PCIR_COMMAND, 2) & ~bit, 2);
	return 0;
}

int
pci_get_max_payload(device_t dev)
{
	int reg;
	if (pci_find_cap(dev, PCIY_EXPRESS, &reg) != 0)
		return 0;
	uint16_t ctl = pci_read_config(dev, reg + 0x8, 2);
	return 128 << ((ctl >> 5) & 0x7);
}

// config space survives here since the device is never powered down
void
pci_save_state(device_t dev)
{
}

void
pci_restore_state(device_t dev)
{
}

// no wake from d3 through pme here
bool
pci_has_pme(device_t dev, int state)
{
	return false;
}

void
pci_clear_pme(device_t dev)
{
}

void
pci_enable_pme(device_t dev)
{
}

uint8_t
pci_get_class(device_t dev)
{
	return pci_read_config(dev, 0x0b, 1);
}

uint8_t
pci_get_subclass(device_t dev)
{
	return pci_read_config(dev, 0x0a, 1);
}
