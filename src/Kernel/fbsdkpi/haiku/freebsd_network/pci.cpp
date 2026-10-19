/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

extern "C" {
#include "device.h"

#include <compat/machine/resource.h>
#include <compat/dev/pci/pcireg.h>
#include <compat/dev/pci/pcivar.h>
}

#include <PCI.h>


//#define DEBUG_PCI
#ifdef DEBUG_PCI
#	define TRACE_PCI(dev, format, args...) device_printf(dev, format , ##args)
#else
#	define TRACE_PCI(dev, format, args...) do { } while (0)
#endif


pci_module_info *gPci;


status_t
init_pci()
{
	if (gPci != NULL)
		return B_OK;

	status_t status = get_module(B_PCI_MODULE_NAME, (module_info **)&gPci);
	if (status != B_OK)
		return status;

	return B_OK;
}


void
uninit_pci()
{
	if (gPci != NULL)
		put_module(B_PCI_MODULE_NAME);
}


pci_info*
get_device_pci_info(device_t device)
{
	struct root_device_softc* root_softc = (struct root_device_softc*)device->root->softc;
	if (root_softc->bus != root_device_softc::BUS_pci)
		return NULL;
	return &root_softc->pci_info;
}


uint32_t
pci_read_config(device_t dev, int offset, int size)
{
	pci_info* info = get_device_pci_info(dev);

	uint32_t value = gPci->read_pci_config(info->bus, info->device,
		info->function, offset, size);
	TRACE_PCI(dev, "pci_read_config(%i, %i) = 0x%x\n", offset, size, value);
	return value;
}


void
pci_write_config(device_t dev, int offset, uint32_t value, int size)
{
	pci_info* info = get_device_pci_info(dev);

	TRACE_PCI(dev, "pci_write_config(%i, 0x%x, %i)\n", offset, value, size);

	gPci->write_pci_config(info->bus, info->device, info->function, offset,
		size, value);
}


uint16_t
pci_get_vendor(device_t dev)
{
	return pci_read_config(dev, PCI_vendor_id, 2);
}


uint16_t
pci_get_device(device_t dev)
{
	return pci_read_config(dev, PCI_device_id, 2);
}


uint16_t
pci_get_subvendor(device_t dev)
{
	return pci_read_config(dev, PCI_subsystem_vendor_id, 2);
}


uint16_t
pci_get_subdevice(device_t dev)
{
	return pci_read_config(dev, PCI_subsystem_id, 2);
}


uint8_t
pci_get_revid(device_t dev)
{
	return pci_read_config(dev, PCI_revision, 1);
}


uint32_t
pci_get_domain(device_t dev)
{
	return 0;
}

uint32_t
pci_get_devid(device_t dev)
{
	return pci_read_config(dev, PCI_device_id, 2) << 16 |
		pci_read_config(dev, PCI_vendor_id, 2);
}

uint8_t
pci_get_cachelnsz(device_t dev)
{
	return pci_read_config(dev, PCI_line_size, 1);
}

uint8_t *
pci_get_ether(device_t dev)
{
	/* used in if_dc to get the MAC from CardBus CIS for Xircom card */
	return NULL; /* NULL is handled in the caller correctly */
}

uint8_t
pci_get_bus(device_t dev)
{
	pci_info *info
		= &((struct root_device_softc *)dev->root->softc)->pci_info;
	return info->bus;
}


uint8_t
pci_get_slot(device_t dev)
{
	pci_info *info
		= &((struct root_device_softc *)dev->root->softc)->pci_info;
	return info->device;
}


uint8_t
pci_get_function(device_t dev)
{
	pci_info* info = get_device_pci_info(dev);
	return info->function;
}


extern "C" device_t pd_pci_peer_at(uint8 bus, uint8 slot, uint8 func);

// PureDarwin: any device in the registry, so drivers can read the host bridge
device_t
pci_find_dbsf(uint32_t domain, uint8_t bus, uint8_t slot, uint8_t func)
{
	if (domain != 0)
		return NULL;
	return pd_pci_peer_at(bus, slot, func);
}


static void
pci_set_command_bit(device_t dev, uint16_t bit)
{
	uint16_t command = pci_read_config(dev, PCI_command, 2);
	pci_write_config(dev, PCI_command, command | bit, 2);
}


int
pci_enable_busmaster(device_t dev)
{
	// We do this a bit later than FreeBSD does.
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0)
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

	pci_set_command_bit(dev, PCI_command_master);
	return 0;
}


int
pci_enable_io(device_t dev, int space)
{
	/* adapted from FreeBSD's pci_enable_io_method */
	int bit = 0;

	switch (space) {
		case SYS_RES_IOPORT:
			bit = PCI_command_io;
			break;
		case SYS_RES_MEMORY:
			bit = PCI_command_memory;
			break;
		default:
			return EINVAL;
	}

	pci_set_command_bit(dev, bit);
	if (pci_read_config(dev, PCI_command, 2) & bit)
		return 0;

	device_printf(dev, "pci_enable_io(%d) failed.\n", space);

	return ENXIO;
}


int
pci_find_cap(device_t dev, int capability, int *capreg)
{
	pci_info* info = get_device_pci_info(dev);
	uint8 offset;
	status_t status;

	status = gPci->find_pci_capability(info->bus, info->device, info->function,
		capability, &offset);
	*capreg = offset;
	return status;
}


int
pci_find_extcap(device_t dev, int capability, int *capreg)
{
	pci_info* info = get_device_pci_info(dev);
	uint16 offset;
	status_t status;

	status = gPci->find_pci_extended_capability(info->bus, info->device, info->function,
		capability, &offset);
	*capreg = offset;
	return status;
}


int
pci_msi_count(device_t dev)
{
	pci_info* info = get_device_pci_info(dev);
	return gPci->get_msi_count(info->bus, info->device, info->function);
}


int
pci_alloc_msi(device_t dev, int *count)
{
	pci_info* info = get_device_pci_info(dev);
	uint32 startVector = 0;
	if (gPci->configure_msi(info->bus, info->device, info->function, *count,
			&startVector) != B_OK) {
		return ENODEV;
	}

	((struct root_device_softc *)dev->root->softc)->is_msi = true;
	info->u.h0.interrupt_line = startVector;
	return EOK;
}


int
pci_release_msi(device_t dev)
{
	pci_info* info = get_device_pci_info(dev);
	gPci->unconfigure_msi(info->bus, info->device, info->function);
	((struct root_device_softc *)dev->root->softc)->is_msi = false;
	((struct root_device_softc *)dev->root->softc)->is_msix = false;
	return EOK;
}


int
pci_msix_table_bar(device_t dev)
{
	pci_info* info = get_device_pci_info(dev);

	uint8 capability_offset;
	if (gPci->find_pci_capability(info->bus, info->device, info->function,
			PCI_cap_id_msix, &capability_offset) != B_OK)
		return -1;

	uint32 table_value = gPci->read_pci_config(info->bus, info->device, info->function,
		capability_offset + PCI_msix_table, 4);

	uint32 bar = table_value & PCI_msix_bir_mask;
	return PCIR_BAR(bar);
}


int
pci_msix_count(device_t dev)
{
	pci_info* info = get_device_pci_info(dev);
	return gPci->get_msix_count(info->bus, info->device, info->function);
}


int
pci_alloc_msix(device_t dev, int *count)
{
	pci_info* info = get_device_pci_info(dev);
	uint32 startVector = 0;
	if (gPci->configure_msix(info->bus, info->device, info->function, *count,
			&startVector) != B_OK) {
		return ENODEV;
	}

	((struct root_device_softc *)dev->root->softc)->is_msix = true;
	info->u.h0.interrupt_line = startVector;
	return EOK;
}


int
pci_get_max_read_req(device_t dev)
{
	int cap;
	uint16_t val;

	if (pci_find_extcap(dev, PCIY_EXPRESS, &cap) != 0)
		return (0);
	val = pci_read_config(dev, cap + PCIR_EXPRESS_DEVICE_CTL, 2);
	val &= PCIM_EXP_CTL_MAX_READ_REQUEST;
	val >>= 12;
	return (1 << (val + 7));
}


int
pci_set_max_read_req(device_t dev, int size)
{
	int cap;
	uint16_t val;

	if (pci_find_extcap(dev, PCIY_EXPRESS, &cap) != 0)
		return (0);
	if (size < 128)
		size = 128;
	if (size > 4096)
		size = 4096;
	size = (1 << (fls(size) - 1));
	val = pci_read_config(dev, cap + PCIR_EXPRESS_DEVICE_CTL, 2);
	val &= ~PCIM_EXP_CTL_MAX_READ_REQUEST;
	val |= (fls(size) - 8) << 12;
	pci_write_config(dev, cap + PCIR_EXPRESS_DEVICE_CTL, val, 2);
	return (size);
}


int
pci_get_powerstate(device_t dev)
{
	int capabilityRegister;
	uint16 status;
	int powerState = PCI_POWERSTATE_D0;

	if (pci_find_extcap(dev, PCIY_PMG, &capabilityRegister) != EOK)
		return powerState;

	status = pci_read_config(dev, capabilityRegister + PCIR_POWER_STATUS, 2);
	switch (status & PCI_pm_mask) {
		case PCI_pm_state_d0:
			break;
		case PCI_pm_state_d1:
			powerState = PCI_POWERSTATE_D1;
			break;
		case PCI_pm_state_d2:
			powerState = PCI_POWERSTATE_D2;
			break;
		case PCI_pm_state_d3:
			powerState = PCI_POWERSTATE_D3;
			break;
		default:
			powerState = PCI_POWERSTATE_UNKNOWN;
			break;
	}

	TRACE_PCI(dev, "%s: D%i\n", __func__, powerState);
	return powerState;
}


int
pci_set_powerstate(device_t dev, int newPowerState)
{
	int capabilityRegister;
	int oldPowerState;
	uint8 currentPowerManagementStatus;
	uint8 newPowerManagementStatus;
	uint16 powerManagementCapabilities;
	bigtime_t stateTransitionDelayInUs = 0;

	if (pci_find_extcap(dev, PCIY_PMG, &capabilityRegister) != EOK)
		return EOPNOTSUPP;

	oldPowerState = pci_get_powerstate(dev);
	if (oldPowerState == newPowerState)
		return EOK;

	switch (max_c(oldPowerState, newPowerState)) {
		case PCI_POWERSTATE_D2:
			stateTransitionDelayInUs = 200;
			break;
		case PCI_POWERSTATE_D3:
			stateTransitionDelayInUs = 10000;
			break;
	}

	currentPowerManagementStatus = pci_read_config(dev, capabilityRegister
		+ PCIR_POWER_STATUS, 2);
	newPowerManagementStatus = currentPowerManagementStatus & ~PCI_pm_mask;
	powerManagementCapabilities = pci_read_config(dev, capabilityRegister
		+ PCIR_POWER_CAP, 2);

	switch (newPowerState) {
		case PCI_POWERSTATE_D0:
			newPowerManagementStatus |= PCIM_PSTAT_D0;
			break;
		case PCI_POWERSTATE_D1:
			if ((powerManagementCapabilities & PCI_pm_d1supp) == 0)
				return EOPNOTSUPP;
			newPowerManagementStatus |= PCIM_PSTAT_D1;
			break;
		case PCI_POWERSTATE_D2:
			if ((powerManagementCapabilities & PCI_pm_d2supp) == 0)
				return EOPNOTSUPP;
			newPowerManagementStatus |= PCIM_PSTAT_D2;
			break;
		case PCI_POWERSTATE_D3:
			newPowerManagementStatus |= PCIM_PSTAT_D3;
			break;
		default:
			return EINVAL;
	}

	TRACE_PCI(dev, "%s: D%i -> D%i\n", __func__, oldPowerState, newPowerState);
	pci_write_config(dev, capabilityRegister + PCIR_POWER_STATUS,
		newPowerManagementStatus, 2);
	if (stateTransitionDelayInUs != 0)
		snooze(stateTransitionDelayInUs);

	return EOK;
}
