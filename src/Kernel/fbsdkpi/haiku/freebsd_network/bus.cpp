/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Copyright 2004, Marcus Overhagen. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */


extern "C" {
#include "device.h"
}

#include <cstdlib>

#include <arch/cpu.h>
#include <interrupts.h>

extern "C" {
#include <compat/dev/pci/pcireg.h>
#include <compat/dev/pci/pcivar.h>
#include <compat/machine/resource.h>
#include <compat/sys/mutex.h>
#include <compat/machine/bus.h>
#include <compat/sys/rman.h>
#include <compat/sys/bus.h>

#include "pd_fbsd_os.h"
#include "pd_haiku_kernel.h"
}


//#define DEBUG_BUS_SPACE_RW
#ifdef DEBUG_BUS_SPACE_RW
#	define TRACE_BUS_SPACE_RW(x) driver_printf x
#else
#	define TRACE_BUS_SPACE_RW(x)
#endif


struct internal_intr {
	device_t		dev;
	driver_filter_t* filter;
	driver_intr_t	*handler;
	void			*arg;
	int				irq;
	uint32			flags;

	thread_id		thread;
	sem_id			sem;
	int32			handling;
};

static int32 intr_wrapper(void *data);


static area_id
map_mem(void **virtualAddr, phys_addr_t _phy, size_t size, uint32 protection,
	const char *name)
{
	uint32 offset = _phy & (B_PAGE_SIZE - 1);
	phys_addr_t physicalAddr = _phy - offset;
	area_id area;

	size = roundup(size + offset, B_PAGE_SIZE);
	area = map_physical_memory(name, physicalAddr, size, B_ANY_KERNEL_ADDRESS,
		protection, virtualAddr);
	if (area < B_OK)
		return area;

	*virtualAddr = (uint8 *)(*virtualAddr) + offset;

	return area;
}


static int
bus_alloc_irq_resource(device_t dev, struct resource *res)
{
	uint8 irq = pci_read_config(dev, PCI_interrupt_line, 1);
	if (irq == 0 || irq == 0xff)
		return -1;

	res->r_bustag = BUS_SPACE_TAG_IRQ;
	res->r_bushandle = irq;
	return 0;
}


static int
bus_alloc_mem_resource(device_t dev, struct resource *res, pci_info *info,
	int bar_index)
{
	phys_addr_t addr = info->u.h0.base_registers[bar_index];
	uint64 size = info->u.h0.base_register_sizes[bar_index];
	uchar flags = info->u.h0.base_register_flags[bar_index];

	// reject empty regions
	if (size == 0)
		return -1;

	// reject I/O space
	if ((flags & PCI_address_space) != 0)
		return -1;

	// TODO: check flags & PCI_address_prefetchable ?

	if ((flags & PCI_address_type) == PCI_address_type_64) {
		addr |= (uint64)info->u.h0.base_registers[bar_index + 1] << 32;
		size |= (uint64)info->u.h0.base_register_sizes[bar_index + 1] << 32;
	}

	// enable this I/O resource
	if (pci_enable_io(dev, SYS_RES_MEMORY) != 0) {
		dprintf("bus_alloc_mem_resource: bar %d, enabling memory decode failed\n",
			bar_index);
		return -1;
	}

	void *virtualAddr;

	// IOPCIFamily maps the BAR itself, which also covers 64-bit windows high
	// above RAM; a mapped area id of -2 marks such a mapping for release
	void* pcidev = pd_pci_iokit_device(info->bus, info->device, info->function);
	if (pcidev != NULL) {
		uint64_t va = pdos_pci_bar_map(pcidev, bar_index, NULL, NULL);
		if (va != 0) {
			res->r_mapped_area = -2;
			res->r_bustag = BUS_SPACE_TAG_MEM;
			res->r_bushandle = (bus_space_handle_t)va;
			return 0;
		}
	}

	res->r_mapped_area = map_mem(&virtualAddr, addr, size,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA, "bus_alloc_resource(MEMORY)");
	if (res->r_mapped_area < B_OK) {
		dprintf("bus_alloc_mem_resource: bar %d at %#" B_PRIx64 " size %#" B_PRIx64
			" did not map (%d)\n", bar_index, (uint64)addr, size, (int)res->r_mapped_area);
		return -1;
	}

	res->r_bustag = BUS_SPACE_TAG_MEM;
	res->r_bushandle = (bus_space_handle_t)virtualAddr;
	return 0;
}


static int
bus_alloc_ioport_resource(device_t dev, struct resource *res, pci_info *info,
	int bar_index)
{
	uint32 size = info->u.h0.base_register_sizes[bar_index];
	uchar flags = info->u.h0.base_register_flags[bar_index];

	// reject empty regions
	if (size == 0)
		return -1;

	// reject memory space
	if ((flags & PCI_address_space) == 0)
		return -1;

	// enable this I/O resource
	if (pci_enable_io(dev, SYS_RES_IOPORT) != 0)
		return -1;

#if !defined(__x86_64__) && !defined(__i386__)
	// no port i/o here, the bar sits in the host bridge's i/o window and is reached as memory
	void* pcidev = pd_pci_iokit_device(info->bus, info->device, info->function);
	uint64_t va = pcidev != NULL ? pdos_pci_bar_map(pcidev, bar_index, NULL, NULL) : 0;
	if (va == 0) {
		dprintf("bus_alloc_ioport_resource: bar %d did not map\n", bar_index);
		return -1;
	}
	res->r_mapped_area = -2;
	res->r_bustag = BUS_SPACE_TAG_MEM;
	res->r_bushandle = (bus_space_handle_t)va;
	return 0;
#endif

	res->r_mapped_area = -1;
	res->r_bustag = BUS_SPACE_TAG_IO;
	res->r_bushandle = info->u.h0.base_registers[bar_index];
	return 0;
}


static int
bus_register_to_bar_index(pci_info *info, int regid)
{
	// check the offset really is of a BAR
	if (regid < PCI_base_registers || (regid % sizeof(uint32) != 0)
		|| (regid >= PCI_base_registers + 6 * (int)sizeof(uint32))) {
		return -1;
	}

	// turn offset into array index
	regid -= PCI_base_registers;
	regid /= sizeof(uint32);
	return regid;
}


struct resource *
bus_alloc_resource(device_t dev, int type, int *rid, unsigned long start,
	unsigned long end, unsigned long count, uint32 flags)
{
	struct resource *res;
	int result = -1;

	if (type != SYS_RES_IRQ && type != SYS_RES_MEMORY
		&& type != SYS_RES_IOPORT)
		return NULL;

	device_printf(dev, "bus_alloc_resource(%i, [%i], 0x%lx, 0x%lx, 0x%lx,"
		"0x%" B_PRIx32 ")\n", type, *rid, start, end, count, flags);

	// maybe a local array of resources is enough
	res = (struct resource *)malloc(sizeof(struct resource));
	if (res == NULL)
		return NULL;

	if (type == SYS_RES_IRQ) {
		if (*rid == 0) {
			// pinned interrupt
			result = bus_alloc_irq_resource(dev, res);
		} else {
			// msi or msi-x interrupt at index *rid - 1
			pci_info* info = get_device_pci_info(dev);
			res->r_bustag = BUS_SPACE_TAG_MSI;
			res->r_bushandle = info->u.h0.interrupt_line + *rid - 1;
			result = 0;
		}
	} else if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		pci_info* info = get_device_pci_info(dev);
		int bar_index = bus_register_to_bar_index(info, *rid);
		if (bar_index >= 0) {
			if (type == SYS_RES_MEMORY)
				result = bus_alloc_mem_resource(dev, res, info, bar_index);
			else
				result = bus_alloc_ioport_resource(dev, res, info, bar_index);
		}
	}

	if (result < 0) {
		free(res);
		return NULL;
	}

	res->r_type = type;
	return res;
}


int
bus_release_resource(device_t dev, int type, int rid, struct resource *res)
{
	if (res == NULL)
		return EINVAL;
	if (res->r_type != type)
		panic("bus_release_resource: mismatch");

	if ((type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) && res->r_mapped_area == -2)
		pdos_unmap_phys((void*)res->r_bushandle, 0);
	else if (type == SYS_RES_MEMORY)
		delete_area(res->r_mapped_area);

	free(res);
	return 0;
}


int
bus_alloc_resources(device_t dev, struct resource_spec *resourceSpec,
	struct resource **resources)
{
	int i;

	for (i = 0; resourceSpec[i].type != -1; i++) {
		resources[i] = bus_alloc_resource_any(dev,
			resourceSpec[i].type, &resourceSpec[i].rid, resourceSpec[i].flags);
		if (resources[i] == NULL
			&& (resourceSpec[i].flags & RF_OPTIONAL) == 0) {
			for (++i; resourceSpec[i].type != -1; i++) {
				resources[i] = NULL;
			}

			bus_release_resources(dev, resourceSpec, resources);
			return ENXIO;
		}
	}
	return 0;
}


void
bus_release_resources(device_t dev, const struct resource_spec *resourceSpec,
	struct resource **resources)
{
	int i;

	for (i = 0; resourceSpec[i].type != -1; i++) {
		if (resources[i] == NULL)
			continue;

		bus_release_resource(dev, resourceSpec[i].type, resourceSpec[i].rid,
			resources[i]);
		resources[i] = NULL;
	}
}


bus_space_handle_t
rman_get_bushandle(struct resource *res)
{
	return res->r_bushandle;
}


bus_space_tag_t
rman_get_bustag(struct resource *res)
{
	return res->r_bustag;
}


int
rman_get_rid(struct resource *res)
{
	return 0;
}


void*
rman_get_virtual(struct resource *res)
{
	return NULL;
}


bus_addr_t
rman_get_start(struct resource *res)
{
	return res->r_bushandle;
}


bus_size_t
rman_get_size(struct resource *res)
{
	area_info info;
	if (get_area_info(res->r_mapped_area, &info) != B_OK)
		return 0;
	return info.size;
}


//	#pragma mark - Interrupt handling


static int32
intr_wrapper(void *data)
{
	struct internal_intr *intr = (struct internal_intr *)data;

	//device_printf(intr->dev, "in interrupt handler.\n");

	if (!HAIKU_CHECK_DISABLE_INTERRUPTS(intr->dev))
		return B_UNHANDLED_INTERRUPT;

	release_sem_etc(intr->sem, 1, B_DO_NOT_RESCHEDULE);
	return intr->handling ? B_HANDLED_INTERRUPT : B_INVOKE_SCHEDULER;
}


static int32
intr_handler(void *data)
{
	struct internal_intr *intr = (struct internal_intr *)data;
	status_t status;

	while (1) {
		status = acquire_sem(intr->sem);
		if (status < B_OK)
			break;

		//device_printf(intr->dev, "in soft interrupt handler.\n");

		atomic_or(&intr->handling, 1);
		if ((intr->flags & INTR_MPSAFE) == 0)
			mtx_lock(&Giant);

		intr->handler(intr->arg);

		if ((intr->flags & INTR_MPSAFE) == 0)
			mtx_unlock(&Giant);
		atomic_and(&intr->handling, 0);
		HAIKU_REENABLE_INTERRUPTS(intr->dev);
	}

	return 0;
}


static void
free_internal_intr(struct internal_intr *intr)
{
	if (intr->sem >= B_OK) {
		status_t status;
		delete_sem(intr->sem);
		wait_for_thread(intr->thread, &status);
	}

	free(intr);
}


int
bus_setup_intr(device_t dev, struct resource *res, int flags,
	driver_filter_t* filter, driver_intr_t handler, void *arg, void **_cookie)
{
	struct internal_intr *intr = (struct internal_intr *)malloc(
		sizeof(struct internal_intr));
	char semName[64];
	status_t status;

	if (intr == NULL)
		return B_NO_MEMORY;

	intr->dev = dev;
	intr->filter = filter;
	intr->handler = handler;
	intr->arg = arg;
	intr->irq = res->r_bushandle;
	intr->flags = flags;
	intr->sem = -1;
	intr->thread = -1;

	if (filter != NULL) {
		status = install_io_interrupt_handler(intr->irq,
			(interrupt_handler)intr->filter, intr->arg, 0);
	} else {
		snprintf(semName, sizeof(semName), "%s intr", dev->device_name);

		intr->sem = create_sem(0, semName);
		if (intr->sem < B_OK) {
			free(intr);
			return B_NO_MEMORY;
		}

		snprintf(semName, sizeof(semName), "%s intr handler", dev->device_name);

		intr->thread = spawn_kernel_thread(intr_handler, semName,
			B_REAL_TIME_DISPLAY_PRIORITY, intr);
		if (intr->thread < B_OK) {
			delete_sem(intr->sem);
			free(intr);
			return B_NO_MEMORY;
		}

		status = install_io_interrupt_handler(intr->irq,
			intr_wrapper, intr, 0);
	}

	if (status == B_OK && res->r_bustag == BUS_SPACE_TAG_MSI) {
		// this is an msi, enable it
		struct root_device_softc* root_softc = ((struct root_device_softc *)dev->root->softc);
		if (root_softc->is_msi) {
			if (gPci->enable_msi(root_softc->pci_info.bus, root_softc->pci_info.device,
					root_softc->pci_info.function) != B_OK) {
				device_printf(dev, "enabling msi failed\n");
				bus_teardown_intr(dev, res, intr);
				return ENODEV;
			}
		} else if (root_softc->is_msix) {
			if (gPci->enable_msix(root_softc->pci_info.bus, root_softc->pci_info.device,
					root_softc->pci_info.function) != B_OK) {
				device_printf(dev, "enabling msix failed\n");
				bus_teardown_intr(dev, res, intr);
				return ENODEV;
			}
		}
	}

	if (status < B_OK) {
		free_internal_intr(intr);
		return status;
	}

	resume_thread(intr->thread);

	*_cookie = intr;
	return 0;
}


int
bus_teardown_intr(device_t dev, struct resource *res, void *arg)
{
	struct internal_intr *intr = (struct internal_intr *)arg;
	if (intr == NULL)
		return -1;

	struct root_device_softc *root = (struct root_device_softc *)dev->root->softc;

	if (root->is_msi || root->is_msix) {
		// disable msi generation
		pci_info *info = &root->pci_info;
		gPci->disable_msi(info->bus, info->device, info->function);
	}

	if (intr->filter != NULL) {
		remove_io_interrupt_handler(intr->irq, (interrupt_handler)intr->filter,
			intr->arg);
	} else {
		remove_io_interrupt_handler(intr->irq, intr_wrapper, intr);
	}

	free_internal_intr(intr);
	return 0;
}


int
bus_bind_intr(device_t dev, struct resource *res, int cpu)
{
	if (dev->parent == NULL)
		return EINVAL;

	// TODO
	return 0;
}


int bus_describe_intr(device_t dev, struct resource *irq, void *cookie,
	const char* fmt, ...)
{
	if (dev->parent == NULL)
		return EINVAL;

	// we don't really support names for interrupts
	return 0;
}


//	#pragma mark - bus functions


// a per-device root tag carrying the IOPCIDevice for dma mappings
bus_dma_tag_t
bus_get_dma_tag(device_t dev)
{
	if (dev == NULL || dev->root == NULL)
		return NULL;

	pci_info* info = get_device_pci_info(dev);
	if (info == NULL)
		return NULL;

	void* pcidev = pd_pci_iokit_device(info->bus, info->device, info->function);
	if (pcidev == NULL)
		return NULL;

	struct root_device_softc* root = (struct root_device_softc*)dev->root->softc;
	if (root->pd_dma_tag == NULL) {
		bus_dma_tag_t tag;
		if (bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
				NULL, NULL, BUS_SPACE_MAXSIZE, BUS_SPACE_UNRESTRICTED,
				BUS_SPACE_MAXSIZE, 0, NULL, NULL, &tag) != 0)
			return NULL;
		pd_bus_dma_tag_set_device(tag, pcidev);
		root->pd_dma_tag = tag;
	}
	return root->pd_dma_tag;
}


int
bus_generic_suspend(device_t dev)
{
	UNIMPLEMENTED();
	return B_ERROR;
}


int
bus_generic_resume(device_t dev)
{
	UNIMPLEMENTED();
	return B_ERROR;
}


void
bus_generic_shutdown(device_t dev)
{
	UNIMPLEMENTED();
}


int
bus_print_child_header(device_t dev, device_t child)
{
	UNIMPLEMENTED();
	return B_ERROR;
}


int
bus_print_child_footer(device_t dev, device_t child)
{
	UNIMPLEMENTED();
	return B_ERROR;
}


int
bus_generic_print_child(device_t dev, device_t child)
{
	UNIMPLEMENTED();
	return B_ERROR;
}


void
bus_generic_driver_added(device_t dev, driver_t *driver)
{
	UNIMPLEMENTED();
}


int
bus_child_present(device_t child)
{
	device_t parent = device_get_parent(child);
	if (parent == NULL)
		return 0;

	return bus_child_present(parent);
}


void
bus_enumerate_hinted_children(device_t bus)
{
#if 0
	UNIMPLEMENTED();
#endif
}
