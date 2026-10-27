// SPDX-License-Identifier: MIT
// haiku's pci bus manager and interrupt api, over the iokit devices the
// driver classes register

#include <OS.h>
#include <KernelExport.h>
#include <PCI.h>
#include <string.h>

#include "pd_fbsd_os.h"
#include "pd_haiku_kernel.h"

#define PD_PCI_SLOTS		8
// each device owns sixteen haiku vectors, the first is its legacy line and
// the rest are msi vectors, so vector numbers fit haiku's uchar line field
#define PD_VECTOR_BASE		32
#define PD_VECTORS_PER_DEV	16

struct pd_pci_dev {
	void		*dev;
	uint8		bus, device, function;
	int			msi_vectors;
};

static struct pd_pci_dev sDevices[PD_PCI_SLOTS];
// devices a driver looks at without attaching, their config space only
#define PD_PCI_PEERS		16
static struct pd_pci_dev sPeers[PD_PCI_PEERS];

static struct pd_pci_dev *
dev_by_location(uint8 bus, uint8 device, uint8 function)
{
	for (int i = 0; i < PD_PCI_SLOTS; i++) {
		struct pd_pci_dev *d = &sDevices[i];
		if (d->dev != NULL && d->bus == bus && d->device == device
				&& d->function == function)
			return d;
	}
	return NULL;
}

static struct pd_pci_dev *
peer_by_location(uint8 bus, uint8 device, uint8 function)
{
	for (int i = 0; i < PD_PCI_PEERS; i++) {
		struct pd_pci_dev *d = &sPeers[i];
		if (d->dev != NULL && d->bus == bus && d->device == device
				&& d->function == function)
			return d;
	}
	return NULL;
}

static int
dev_index(struct pd_pci_dev *d)
{
	return (int)(d - sDevices);
}

static uint8
legacy_vector(struct pd_pci_dev *d)
{
	return PD_VECTOR_BASE + dev_index(d) * PD_VECTORS_PER_DEV;
}

static uint32
pci_read(uint8 bus, uint8 device, uint8 function, uint16 offset, uint8 size)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL) {
		d = peer_by_location(bus, device, function);
		return d != NULL ? pdos_pci_read(d->dev, offset, size) : 0xffffffff;
	}
	// drivers read the interrupt line and get the haiku vector for it
	if (offset == PCI_interrupt_line && size == 1)
		return legacy_vector(d);
	return pdos_pci_read(d->dev, offset, size);
}

static void
pci_write(uint8 bus, uint8 device, uint8 function, uint16 offset, uint8 size,
	uint32 value)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL) {
		d = peer_by_location(bus, device, function);
		if (d != NULL)
			pdos_pci_write(d->dev, offset, size, value);
		return;
	}
	if (offset == PCI_interrupt_line && size == 1)
		return;
	pdos_pci_write(d->dev, offset, size, value);
}

static status_t
find_capability(uchar bus, uchar device, uchar function, uchar cap_id,
	uchar *offset)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL)
		return B_BAD_VALUE;
	if ((pdos_pci_read(d->dev, PCI_status, 2) & PCI_status_capabilities) == 0)
		return B_ENTRY_NOT_FOUND;
	uint8 ptr = pdos_pci_read(d->dev, PCI_capabilities_ptr, 1) & ~3;
	for (int guard = 0; ptr != 0 && guard < 48; guard++) {
		if (pdos_pci_read(d->dev, ptr, 1) == cap_id) {
			if (offset != NULL)
				*offset = ptr;
			return B_OK;
		}
		ptr = pdos_pci_read(d->dev, ptr + 1, 1) & ~3;
	}
	return B_ENTRY_NOT_FOUND;
}

static status_t
find_extended_capability(uint8 bus, uint8 device, uint8 function,
	uint16 cap_id, uint16 *offset)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL)
		return B_BAD_VALUE;
	uint16 ptr = 0x100;
	for (int guard = 0; ptr != 0 && guard < 128; guard++) {
		uint32 header = pdos_pci_read(d->dev, ptr, 4);
		if (header == 0 || header == 0xffffffff)
			break;
		if ((header & 0xffff) == cap_id) {
			if (offset != NULL)
				*offset = ptr;
			return B_OK;
		}
		ptr = (header >> 20) & 0xffc;
	}
	return B_ENTRY_NOT_FOUND;
}

static status_t
reserve_device(uchar bus, uchar device, uchar function, const char *name,
	void *cookie)
{
	return B_OK;
}

static status_t
update_interrupt_line(uchar bus, uchar device, uchar function, uchar line)
{
	return B_OK;
}

static status_t
get_powerstate(uint8 bus, uint8 device, uint8 function, uint8 *state)
{
	*state = PCI_pm_state_d0;
	return B_OK;
}

static status_t
set_powerstate(uint8 bus, uint8 device, uint8 function, uint8 state)
{
	return B_OK;
}

// msi and msi-x through IOPCIFamily's interrupt indexes

static uint32
get_msi_count(uint8 bus, uint8 device, uint8 function)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL)
		return 0;
	int count = pdos_pci_msi_count(d->dev);
	return count > PD_VECTORS_PER_DEV - 1 ? PD_VECTORS_PER_DEV - 1 : count;
}

static status_t
configure_msi(uint8 bus, uint8 device, uint8 function, uint32 count,
	uint32 *startVector)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL || count == 0 || count > get_msi_count(bus, device, function))
		return B_BAD_VALUE;
	d->msi_vectors = count;
	*startVector = legacy_vector(d) + 1;
	return B_OK;
}

static status_t
unconfigure_msi(uint8 bus, uint8 device, uint8 function)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d != NULL)
		d->msi_vectors = 0;
	return B_OK;
}

static status_t
msi_noop(uint8 bus, uint8 device, uint8 function)
{
	return B_OK;
}

static uint32
get_msix_count(uint8 bus, uint8 device, uint8 function)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL)
		return 0;
	int count = pdos_pci_msix_count(d->dev);
	return count > PD_VECTORS_PER_DEV - 1 ? PD_VECTORS_PER_DEV - 1 : count;
}

static status_t
configure_msix(uint8 bus, uint8 device, uint8 function, uint32 count,
	uint32 *startVector)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	if (d == NULL || count == 0 || count > get_msix_count(bus, device, function))
		return B_BAD_VALUE;
	d->msi_vectors = count;
	*startVector = legacy_vector(d) + 1;
	return B_OK;
}

static long
get_nth_pci_info(long index, pci_info *info)
{
	return B_ERROR;
}

static pci_module_info sPciModule = {
	.binfo = { .minfo = { B_PCI_MODULE_NAME, 0, NULL } },
	.get_nth_pci_info = get_nth_pci_info,
	.read_pci_config = pci_read,
	.write_pci_config = pci_write,
	.find_pci_capability = find_capability,
	.reserve_device = reserve_device,
	.unreserve_device = reserve_device,
	.update_interrupt_line = update_interrupt_line,
	.find_pci_extended_capability = find_extended_capability,
	.get_powerstate = get_powerstate,
	.set_powerstate = set_powerstate,
	.get_msi_count = get_msi_count,
	.configure_msi = configure_msi,
	.unconfigure_msi = unconfigure_msi,
	.enable_msi = msi_noop,
	.disable_msi = msi_noop,
	.get_msix_count = get_msix_count,
	.configure_msix = configure_msix,
	.enable_msix = msi_noop,
};

pci_module_info *
pd_pci_module(void)
{
	return &sPciModule;
}

// fills the bars by reading and sizing them, since haiku's pci_info carries
// the host addresses the compat bus code maps
static void
fill_bars(void *dev, pci_info *info)
{
	uint16 command = pdos_pci_read(dev, PCI_command, 2);
	pdos_pci_write(dev, PCI_command, 2,
		command & ~(PCI_command_io | PCI_command_memory));
	for (int bar = 0; bar < 6; bar++) {
		uint16 reg = PCI_base_registers + bar * 4;
		uint32 value = pdos_pci_read(dev, reg, 4);
		pdos_pci_write(dev, reg, 4, 0xffffffff);
		uint32 mask = pdos_pci_read(dev, reg, 4);
		pdos_pci_write(dev, reg, 4, value);

		info->u.h0.base_registers_pci[bar] = value;
		if (mask == 0 || mask == 0xffffffff) {
			info->u.h0.base_registers[bar] = 0;
			info->u.h0.base_register_sizes[bar] = 0;
			info->u.h0.base_register_flags[bar] = 0;
			continue;
		}
		if ((value & PCI_address_space) != 0) {
			info->u.h0.base_registers[bar] = value & PCI_address_io_mask;
			info->u.h0.base_register_sizes[bar] = (~(mask & PCI_address_io_mask) + 1) & 0xffff;
			info->u.h0.base_register_flags[bar] = value & ~PCI_address_io_mask;
			continue;
		}
		info->u.h0.base_registers[bar] = value & PCI_address_memory_32_mask;
		info->u.h0.base_register_sizes[bar] = ~(mask & PCI_address_memory_32_mask) + 1;
		info->u.h0.base_register_flags[bar] = value & ~PCI_address_memory_32_mask;
		if ((value & PCI_address_type) == PCI_address_type_64 && bar < 5) {
			// the upper half lives in the next register, sized the same way
			uint32 high = pdos_pci_read(dev, reg + 4, 4);
			pdos_pci_write(dev, reg + 4, 4, 0xffffffff);
			uint32 highMask = pdos_pci_read(dev, reg + 4, 4);
			pdos_pci_write(dev, reg + 4, 4, high);
			// both halves size one 64-bit window, a 4 GB or larger bar has no bits in the low half
			uint64 size64 = ~(((uint64)highMask << 32) | (mask & PCI_address_memory_32_mask)) + 1;
			info->u.h0.base_register_sizes[bar] = (uint32)size64;
			bar++;
			info->u.h0.base_registers_pci[bar] = high;
			info->u.h0.base_registers[bar] = high;
			info->u.h0.base_register_sizes[bar] = (uint32)(size64 >> 32);
			info->u.h0.base_register_flags[bar] = 0;
		}
	}
	pdos_pci_write(dev, PCI_command, 2, command | PCI_command_memory | PCI_command_master);
}

status_t
pd_pci_register(void *dev, pci_info *info)
{
	struct pd_pci_dev *d = NULL;
	for (int i = 0; i < PD_PCI_SLOTS; i++) {
		if (sDevices[i].dev == NULL) {
			d = &sDevices[i];
			break;
		}
	}
	if (d == NULL)
		return B_NO_MEMORY;

	uint8 bus = 0, device = 0, function = 0;
	pdos_pci_location(dev, &bus, &device, &function);
	pdos_pci_note(dev);
	d->dev = dev;
	d->bus = bus;
	d->device = device;
	d->function = function;
	d->msi_vectors = 0;

	memset(info, 0, sizeof(*info));
	info->vendor_id = pdos_pci_read(dev, PCI_vendor_id, 2);
	info->device_id = pdos_pci_read(dev, PCI_device_id, 2);
	info->bus = bus;
	info->device = device;
	info->function = function;
	info->revision = pdos_pci_read(dev, PCI_revision, 1);
	info->class_api = pdos_pci_read(dev, PCI_class_api, 1);
	info->class_sub = pdos_pci_read(dev, PCI_class_sub, 1);
	info->class_base = pdos_pci_read(dev, PCI_class_base, 1);
	info->line_size = pdos_pci_read(dev, PCI_line_size, 1);
	info->latency = pdos_pci_read(dev, PCI_latency, 1);
	info->header_type = pdos_pci_read(dev, PCI_header_type, 1);
	info->u.h0.subsystem_id = pdos_pci_read(dev, PCI_subsystem_id, 2);
	info->u.h0.subsystem_vendor_id = pdos_pci_read(dev, PCI_subsystem_vendor_id, 2);
	info->u.h0.interrupt_line = legacy_vector(d);
	info->u.h0.interrupt_pin = pdos_pci_read(dev, PCI_interrupt_pin, 1);
	fill_bars(dev, info);
	return B_OK;
}

// a device the driver did not attach to: its pci_info and config access, no bars
// are touched and the command register is left as it is
status_t
pd_pci_add_peer(void *dev, pci_info *info)
{
	uint8 bus = 0, device = 0, function = 0;
	struct pd_pci_dev *d;

	pdos_pci_location(dev, &bus, &device, &function);
	d = peer_by_location(bus, device, function);
	if (d == NULL) {
		for (int i = 0; i < PD_PCI_PEERS && d == NULL; i++) {
			if (sPeers[i].dev == NULL)
				d = &sPeers[i];
		}
		if (d == NULL)
			return B_NO_MEMORY;
		d->dev = dev;
		d->bus = bus;
		d->device = device;
		d->function = function;
	}

	memset(info, 0, sizeof(*info));
	info->vendor_id = pdos_pci_read(dev, PCI_vendor_id, 2);
	info->device_id = pdos_pci_read(dev, PCI_device_id, 2);
	info->bus = bus;
	info->device = device;
	info->function = function;
	info->revision = pdos_pci_read(dev, PCI_revision, 1);
	info->class_api = pdos_pci_read(dev, PCI_class_api, 1);
	info->class_sub = pdos_pci_read(dev, PCI_class_sub, 1);
	info->class_base = pdos_pci_read(dev, PCI_class_base, 1);
	info->header_type = pdos_pci_read(dev, PCI_header_type, 1);
	return B_OK;
}

void
pd_pci_unregister(void *dev)
{
	for (int i = 0; i < PD_PCI_SLOTS; i++) {
		if (sDevices[i].dev == dev)
			sDevices[i].dev = NULL;
	}
}

// interrupts

struct pd_handler {
	int32				vector;
	interrupt_handler	handler;
	void				*data;
	void				*cookie;
};

#define PD_HANDLER_SLOTS	(PD_PCI_SLOTS * PD_VECTORS_PER_DEV)
static struct pd_handler sHandlers[PD_HANDLER_SLOTS];

static int
call_handler(void *arg)
{
	struct pd_handler *h = arg;
	return h->handler(h->data) != B_UNHANDLED_INTERRUPT;
}

status_t
install_io_interrupt_handler(int32 vector, interrupt_handler handler,
	void *data, uint32 flags)
{
	int devIndex = (vector - PD_VECTOR_BASE) / PD_VECTORS_PER_DEV;
	int index = (vector - PD_VECTOR_BASE) % PD_VECTORS_PER_DEV;
	if (vector < PD_VECTOR_BASE || devIndex >= PD_PCI_SLOTS
			|| sDevices[devIndex].dev == NULL)
		return B_BAD_VALUE;

	struct pd_handler *h = NULL;
	for (int i = 0; i < PD_HANDLER_SLOTS; i++) {
		if (sHandlers[i].handler == NULL) {
			h = &sHandlers[i];
			break;
		}
	}
	if (h == NULL)
		return B_NO_MEMORY;
	h->vector = vector;
	h->handler = handler;
	h->data = data;
	h->cookie = pdos_intr_install(sDevices[devIndex].dev, index, call_handler, h);
	if (h->cookie == NULL) {
		h->handler = NULL;
		return B_ERROR;
	}
	return B_OK;
}

status_t
remove_io_interrupt_handler(int32 vector, interrupt_handler handler, void *data)
{
	for (int i = 0; i < PD_HANDLER_SLOTS; i++) {
		struct pd_handler *h = &sHandlers[i];
		if (h->handler == handler && h->data == data && h->vector == vector) {
			pdos_intr_remove(h->cookie);
			h->handler = NULL;
			return B_OK;
		}
	}
	return B_BAD_VALUE;
}

// the IOPCIDevice behind a registered location, for dma mappings
void *
pd_pci_iokit_device(uint8 bus, uint8 device, uint8 function)
{
	struct pd_pci_dev *d = dev_by_location(bus, device, function);
	return d != NULL ? d->dev : NULL;
}

// device tree nubs share the vector table with pci devices: a slot, then 16 vectors per nub
int
pd_platform_nub_vectors(void *nub)
{
	for (int i = 0; i < PD_PCI_SLOTS; i++) {
		if (sDevices[i].dev == nub)
			return PD_VECTOR_BASE + i * PD_VECTORS_PER_DEV;
	}
	for (int i = 0; i < PD_PCI_SLOTS; i++) {
		if (sDevices[i].dev == NULL) {
			sDevices[i].dev = nub;
			// no pci location: lookups by bus, device and function never find it
			sDevices[i].bus = 0xff;
			sDevices[i].device = 0xff;
			sDevices[i].function = 0xff;
			return PD_VECTOR_BASE + i * PD_VECTORS_PER_DEV;
		}
	}
	return -1;
}
