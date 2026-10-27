// SPDX-License-Identifier: MIT
// the pci side of linuxkpi drivers: the driver list linux_pci.c registers
// into, and the pci_devinfo each device carries in its ivars

#include <stdlib.h>
#include <string.h>

#include "device.h"

#include <compat/sys/haiku-module.h>
#include <compat/sys/bus.h>
#include <compat/sys/rman.h>
#include <compat/sys/bus_dma.h>
#include <compat/dev/pci/pcivar.h>

#include "fbsd-extra/dev/pci/pci_private.h"
#include "pd_fbsd_os.h"

#define PCIR_BAR(x)		(0x10 + (x) * 4)
#define PCIY_MSI		0x05
#define PCIY_EXPRESS	0x10
#define PCIR_MSI_CTRL	0x2
#define PCIR_COMMAND	0x04
#define PCIM_CMD_PORTEN	0x0001
#define PCIM_CMD_MEMEN	0x0002
#define PCIM_CMD_BUSMASTEREN	0x0004
#define PCIER_FLAGS		0x2
#define PCIEM_FLAGS_TYPE	0x00f0
#define PCI_BASE_ADDRESS_SPACE_IO	0x01
#define PCI_BASE_ADDRESS_TYPE_64	0x04

#define PD_LKPI_MAX_DRIVERS	4
#define PD_LKPI_MAX_ROOTS	24

extern void (*pd_fbsd_prepare_device)(device_t dev);

static driver_t *sDrivers[PD_LKPI_MAX_DRIVERS + 1];
static int sDriverCount;

static struct {
	device_t root;
	struct pci_devinfo *dinfo;
} sInfos[PD_LKPI_MAX_ROOTS];

static struct pci_devinfo *
devinfo_for_root(device_t root)
{
	int free_slot = -1;
	for (int i = 0; i < PD_LKPI_MAX_ROOTS; i++) {
		if (sInfos[i].root == root)
			return sInfos[i].dinfo;
		if (sInfos[i].root == NULL && free_slot < 0)
			free_slot = i;
	}
	if (free_slot < 0)
		return NULL;

	struct pci_devinfo *dinfo = calloc(1, sizeof(*dinfo));
	if (dinfo == NULL)
		return NULL;
	sInfos[free_slot].root = root;
	sInfos[free_slot].dinfo = dinfo;
	return dinfo;
}

// fills the devinfo from the device's pci_info and config space
static void
fill_devinfo(device_t dev, struct pci_devinfo *dinfo)
{
	pci_info *info = get_device_pci_info(dev);
	struct resource_list *rl = &dinfo->resources;

	memset(dinfo, 0, sizeof(*dinfo));
	dinfo->cfg.vendor = info->vendor_id;
	dinfo->cfg.device = info->device_id;
	dinfo->cfg.bus = info->bus;
	dinfo->cfg.slot = info->device;
	dinfo->cfg.func = info->function;

	for (int bar = 0; bar < 6; bar++) {
		uint64_t size = info->u.h0.base_register_sizes[bar];
		uint8 flags = info->u.h0.base_register_flags[bar];
		uint64_t base = info->u.h0.base_registers[bar];
		bool is64 = (flags & PCI_BASE_ADDRESS_TYPE_64) != 0
			&& (flags & PCI_BASE_ADDRESS_SPACE_IO) == 0;
		// a 64-bit bar's size spans both halves, a resizable bar of 16 GB has none in the low one
		if (is64 && bar < 5) {
			base |= (uint64_t)info->u.h0.base_registers[bar + 1] << 32;
			size |= (uint64_t)info->u.h0.base_register_sizes[bar + 1] << 32;
		}

		dinfo->bars[bar].pm_reg = PCIR_BAR(bar);
		dinfo->bars[bar].pm_value = base | (flags & 0xf);
		dinfo->bars[bar].pm_size = size;
		if (size != 0 && rl->count < PD_RESOURCE_LIST_MAX) {
			struct resource_list_entry *rle = &rl->entries[rl->count++];
			rle->type = (flags & PCI_BASE_ADDRESS_SPACE_IO) != 0
				? SYS_RES_IOPORT : SYS_RES_MEMORY;
			rle->rid = PCIR_BAR(bar);
			rle->start = base;
			rle->count = size;
			rle->end = base + size - 1;
		}
		if (is64)
			bar++;
	}

	if (rl->count < PD_RESOURCE_LIST_MAX) {
		struct resource_list_entry *rle = &rl->entries[rl->count++];
		rle->type = SYS_RES_IRQ;
		rle->rid = 0;
		rle->start = rle->end = info->u.h0.interrupt_line;
		rle->count = 1;
	}

	int reg;
	if (pci_find_cap(dev, PCIY_MSI, &reg) == 0) {
		dinfo->cfg.msi.msi_location = reg;
		dinfo->cfg.msi.msi_ctrl = pci_read_config(dev, reg + PCIR_MSI_CTRL, 2);
	}
	if (pci_find_cap(dev, PCIY_EXPRESS, &reg) == 0) {
		dinfo->cfg.pcie.pcie_location = reg;
		dinfo->cfg.pcie.pcie_flags = pci_read_config(dev, reg + PCIER_FLAGS, 2);
		dinfo->cfg.pcie.pcie_type = dinfo->cfg.pcie.pcie_flags & PCIEM_FLAGS_TYPE;
	}
}

static void
lkpi_prepare_device(device_t dev)
{
	struct pci_devinfo *dinfo = devinfo_for_root(dev->root);
	if (dinfo == NULL)
		return;

	fill_devinfo(dev, dinfo);
	device_set_ivars(dev, dinfo);
	dprintf("%s: msi %d, msi-x %d vectors offered\n", gDriverName, pci_msi_count(dev),
		pci_msix_count(dev));
}

int
pd_lkpi_pci_driver_add(struct driver *driver)
{
	if (sDriverCount >= PD_LKPI_MAX_DRIVERS)
		return ENOSPC;

	sDrivers[sDriverCount++] = driver;
	sDrivers[sDriverCount] = NULL;
	pd_fbsd_prepare_device = lkpi_prepare_device;
	return 0;
}

// what a linuxkpi kext's __haiku_init_hardware calls
void
pd_lkpi_init_hardware(void)
{
	dprintf("%s: %d linuxkpi pci driver(s) registered\n", gDriverName, sDriverCount);
	if (sDriverCount != 0) {
		status_t status = _fbsd_init_hardware_pci(sDrivers);
		if (status != B_OK)
			dprintf("%s: pci attach returned %d\n", gDriverName, (int)status);
	}
}

struct pci_map *
pci_find_bar(device_t dev, int reg)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	int bar = (reg - PCIR_BAR(0)) / 4;
	if (dinfo == NULL || bar < 0 || bar >= 6 || dinfo->bars[bar].pm_size == 0)
		return NULL;
	return &dinfo->bars[bar];
}

struct resource_list_entry *
resource_list_find(struct resource_list *rl, int type, int rid)
{
	for (int i = 0; i < rl->count; i++) {
		if (rl->entries[i].type == type && rl->entries[i].rid == rid)
			return &rl->entries[i];
	}
	return NULL;
}

struct resource *
pci_reserve_map(device_t dev, device_t child, int type, int rid,
	rman_res_t start, rman_res_t end, rman_res_t count, u_int num, u_int flags)
{
	struct pci_devinfo *dinfo = device_get_ivars(child);
	struct resource_list_entry *rle;

	if (dinfo == NULL || (rle = resource_list_find(&dinfo->resources, type, rid)) == NULL)
		return NULL;
	if (rle->res == NULL)
		rle->res = bus_alloc_resource_any(child, type, &rid, RF_ACTIVE);
	return rle->res;
}

int
bus_translate_resource(device_t dev, int type, rman_res_t start, rman_res_t *newstart)
{
	*newstart = start;
	return 0;
}

int
pd_lkpi_pci_get_rid(device_t dev, uintptr_t *rid)
{
	pci_info *info = get_device_pci_info(dev);
	*rid = ((uintptr_t)info->bus << 8) | ((uintptr_t)info->device << 3)
		| info->function;
	return 0;
}

// pci helpers linux/pci.h reaches for, on the one device of this kext

bool
is_pci_device(device_t dev)
{
	return true;
}

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

bool
pci_has_pme(device_t dev, int state)
{
	return false;
}

// other devices on the bus, read through their config space only
device_t pd_pci_peer_at(uint8 bus, uint8 slot, uint8 func);
device_t pd_pci_peer_next(device_t from, int (*match)(const pci_info *info, void *arg), void *arg);

device_t
pci_find_bsf(uint8_t bus, uint8_t slot, uint8_t func)
{
	return pd_pci_peer_at(bus, slot, func);
}

static int
match_ids(const pci_info *info, void *arg)
{
	const uint16_t *ids = arg;
	return info->vendor_id == ids[0] && info->device_id == ids[1];
}

device_t
pci_find_device(uint16_t vendor, uint16_t device)
{
	uint16_t ids[2] = { vendor, device };
	return pd_pci_peer_next(NULL, match_ids, ids);
}

static int
match_class(const pci_info *info, void *arg)
{
	const uint8_t *c = arg;
	return info->class_base == c[0] && info->class_sub == c[1];
}

device_t
pci_find_class_from(uint8_t class, uint8_t subclass, device_t devfrom)
{
	uint8_t c[2] = { class, subclass };
	return pd_pci_peer_next(devfrom, match_class, c);
}

static int
match_base_class(const pci_info *info, void *arg)
{
	return info->class_base == *(const uint8_t *)arg;
}

device_t
pci_find_base_class_from(uint8_t class, device_t devfrom)
{
	return pd_pci_peer_next(devfrom, match_base_class, &class);
}

device_t
pci_find_pcie_root_port(device_t dev)
{
	return NULL;
}

// the expansion rom base register and the display base class
#define PD_PCIR_ROM	0x30
#define PD_PCIR_CLASS	0x0b
#define PD_PCIC_DISPLAY	0x03

// the display device under dev (linuxkpi passes the gpu's parent), or dev itself
static device_t
pd_vga_device(device_t dev)
{
	device_t *kids, found = NULL;
	int count, i;

	if (device_get_ivars(dev) != NULL && pci_read_config(dev, PD_PCIR_CLASS, 1) == PD_PCIC_DISPLAY)
		return dev;
	if (device_get_children(dev, &kids, &count) != 0)
		return NULL;
	for (i = 0; i < count && found == NULL; i++) {
		if (device_get_ivars(kids[i]) != NULL &&
		    pci_read_config(kids[i], PD_PCIR_CLASS, 1) == PD_PCIC_DISPLAY)
			found = kids[i];
	}
	free(kids, M_TEMP);
	return found;
}

static size_t pd_vga_bios_size;

// maps the expansion rom the firmware (or a vm's romfile) placed, the caller enabled its decode
void *
vga_pci_map_bios(device_t dev, size_t *size)
{
	device_t vga = pd_vga_device(dev);
	uint32_t bar, sized;
	size_t len;
	void *rom;

	if (vga == NULL) {
		printf("pd: vga rom, no display device under %s\n", device_get_nameunit(dev));
		return NULL;
	}
	bar = pci_read_config(vga, PD_PCIR_ROM, 4);
	printf("pd: vga rom bar 0x%08x\n", bar);
	if ((bar & ~0x7ffu) == 0)
		return NULL;
	pci_write_config(vga, PD_PCIR_ROM, 0xfffff800u | (bar & 1), 4);
	sized = pci_read_config(vga, PD_PCIR_ROM, 4) & ~0x7ffu;
	pci_write_config(vga, PD_PCIR_ROM, bar, 4);
	if (sized == 0)
		return NULL;
	len = (size_t)(~sized + 1);
	rom = pdos_map_phys(bar & ~0x7ffu, len, 1);
	if (rom == NULL)
		return NULL;
	printf("pd: vga rom %zu bytes at 0x%08x, signature %02x %02x\n", len, bar & ~0x7ffu,
	    ((uint8_t *)rom)[0], ((uint8_t *)rom)[1]);
	pd_vga_bios_size = len;
	*size = len;
	return rom;
}

void
vga_pci_unmap_bios(device_t dev, void *bios)
{
	if (bios != NULL)
		pdos_unmap_phys(bios, pd_vga_bios_size);
}

// newbus pieces linux_pci.c names for drm and rescans

devclass_t
devclass_find(const char *classname)
{
	return NULL;
}

devclass_t
devclass_create(const char *classname)
{
	return NULL;
}

int
devclass_delete_driver(devclass_t busclass, driver_t *driver)
{
	return 0;
}

driver_t *
device_get_driver(device_t dev)
{
	return dev->driver;
}

int
device_probe_and_attach(device_t dev)
{
	return ENXIO;
}

void
bus_attach_children(device_t dev)
{
}

void
bus_identify_children(device_t dev)
{
}

int
bus_get_domain(device_t dev, int *domain)
{
	*domain = 0;
	return 0;
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

device_t
devclass_get_device(devclass_t dc, int unit)
{
	return NULL;
}

// message interrupts get irq resources rid 1..count, numbered above any
// pin interrupt so linuxkpi can tell them apart
#define PD_LKPI_MSI_IRQ_BASE	256

void
pd_lkpi_pci_msi_added(device_t dev, int count)
{
	struct pci_devinfo *dinfo = device_get_ivars(dev);
	if (dinfo == NULL)
		return;

	struct resource_list *rl = &dinfo->resources;
	for (int rid = 1; rid <= count; rid++) {
		struct resource_list_entry *rle = resource_list_find(rl, SYS_RES_IRQ, rid);
		if (rle == NULL) {
			if (rl->count >= PD_RESOURCE_LIST_MAX)
				return;
			rle = &rl->entries[rl->count++];
		}
		rle->type = SYS_RES_IRQ;
		rle->rid = rid;
		rle->start = rle->end = PD_LKPI_MSI_IRQ_BASE + rid - 1;
		rle->count = 1;
		rle->res = NULL;
	}
}
