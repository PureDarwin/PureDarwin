/*******************************************************************************
/
/	File:		PCI.h
/
/	Description:	Interface to the PCI bus.
/	For more information, see "PCI Local Bus Specification, Revision 2.1",
/	PCI Special Interest Group, 1995.
/
/	Copyright 1993-98, Be Incorporated, All Rights Reserved.
/
*******************************************************************************/


#ifndef _PCI_H
#define _PCI_H

//#include <BeBuild.h>
//#include <SupportDefs.h>
#include <bus_manager.h>

#ifdef __cplusplus
extern "C" {
#endif


/* -----
	pci device info
----- */

typedef struct pci_info {
	ushort	vendor_id;				/* vendor id */
	ushort	device_id;				/* device id */
	uchar	bus;					/* bus number */
	uchar	device;					/* device number on bus */
	uchar	function;				/* function number in device */
	uchar	revision;				/* revision id */
	uchar	class_api;				/* specific register interface type */
	uchar	class_sub;				/* specific device function */
	uchar	class_base;				/* device type (display vs network, etc) */
	uchar	line_size;				/* cache line size in 32 bit words */
	uchar	latency;				/* latency timer */
	uchar	header_type;			/* header type */
	uchar	bist;					/* built-in self-test */
	uchar	reserved;				/* filler, for alignment */
	union {
		struct {
			uint32	cardbus_cis;			/* CardBus CIS pointer */
			ushort	subsystem_id;			/* subsystem (add-in card) id */
			ushort	subsystem_vendor_id;	/* subsystem (add-in card) vendor id */
			uint32	rom_base;				/* rom base address, viewed from host */
			uint32	rom_base_pci;			/* rom base addr, viewed from pci */
			uint32	rom_size;				/* rom size */
			uint32	base_registers[6];		/* base registers, viewed from host */
			uint32	base_registers_pci[6];	/* base registers, viewed from pci */
			uint32	base_register_sizes[6];	/* size of what base regs point to */
			uchar	base_register_flags[6];	/* flags from base address fields */
			uchar	interrupt_line;			/* interrupt line */
			uchar	interrupt_pin;			/* interrupt pin */
			uchar	min_grant;				/* burst period @ 33 Mhz */
			uchar	max_latency;			/* how often PCI access needed */
		} h0;
		struct {
			uint32	base_registers[2];		/* base registers, viewed from host */
			uint32	base_registers_pci[2];	/* base registers, viewed from pci */
			uint32	base_register_sizes[2];	/* size of what base regs point to */
			uchar	base_register_flags[2];	/* flags from base address fields */
			uchar	primary_bus;
			uchar	secondary_bus;
			uchar	subordinate_bus;
			uchar	secondary_latency;
			uchar	io_base;
			uchar	io_limit;
			ushort	secondary_status;
			ushort	memory_base;
			ushort	memory_limit;
			ushort  prefetchable_memory_base;
			ushort  prefetchable_memory_limit;
			uint32	prefetchable_memory_base_upper32;
			uint32	prefetchable_memory_limit_upper32;
			ushort	io_base_upper16;
			ushort	io_limit_upper16;
			uint32	rom_base;				/* rom base address, viewed from host */
			uint32	rom_base_pci;			/* rom base addr, viewed from pci */
			uchar	interrupt_line;			/* interrupt line */
			uchar	interrupt_pin;			/* interrupt pin */
			ushort	bridge_control;
			ushort	subsystem_id;			/* subsystem (add-in card) id */
			ushort	subsystem_vendor_id;	/* subsystem (add-in card) vendor id */
		} h1;
		struct {
			ushort	subsystem_id;			/* subsystem (add-in card) id */
			ushort	subsystem_vendor_id;	/* subsystem (add-in card) vendor id */

#ifdef __HAIKU_PCI_BUS_MANAGER_TESTING
			// for testing only, not final (do not use!):
			uchar   primary_bus;
			uchar   secondary_bus;
			uchar   subordinate_bus;
			uchar   secondary_latency;
			ushort  reserved;
			uint32  memory_base;
			uint32  memory_limit;
			uint32  memory_base_upper32;
			uint32  memory_limit_upper32;
			uint32  io_base;
			uint32  io_limit;
			uint32  io_base_upper32;
			uint32  io_limit_upper32;
			ushort  secondary_status;
			ushort  bridge_control;
#endif /* __HAIKU_PCI_BUS_MANAGER_TESTING */
		} h2;
	} u;
} pci_info;


typedef struct pci_module_info pci_module_info;

struct pci_module_info {
	bus_manager_info	binfo;

	uint8			(*read_io_8) (int mapped_io_addr);
	void			(*write_io_8) (int mapped_io_addr, uint8 value);
	uint16			(*read_io_16) (int mapped_io_addr);
	void			(*write_io_16) (int mapped_io_addr, uint16 value);
	uint32			(*read_io_32) (int mapped_io_addr);
	void			(*write_io_32) (int mapped_io_addr, uint32 value);

	long			(*get_nth_pci_info) (
						long		index,	/* index into pci device table */
						pci_info 	*info	/* caller-supplied buffer for info */
					);
	uint32			(*read_pci_config) (
						uint8	bus,		/* bus number */
						uint8	device,		/* device # on bus */
						uint8	function,	/* function # in device */
						uint16	offset,		/* offset in configuration space */
						uint8	size		/* # bytes to read (1, 2 or 4) */
					);
	void			(*write_pci_config) (
						uint8	bus,		/* bus number */
						uint8	device,		/* device # on bus */
						uint8	function,	/* function # in device */
						uint16	offset,		/* offset in configuration space */
						uint8	size,		/* # bytes to write (1, 2 or 4) */
						uint32	value		/* value to write */
					);

	phys_addr_t		(*ram_address) (phys_addr_t physical_address_in_system_memory);

	status_t		(*find_pci_capability) (
						uchar	bus,
						uchar	device,
						uchar	function,
						uchar	cap_id,
						uchar	*offset
					);

	status_t		(*reserve_device) (
						uchar bus,
						uchar device,
						uchar function,
						const char *driver_name,
						void *cookie);
	status_t		(*unreserve_device) (
						uchar bus,
						uchar device,
						uchar function,
						const char *driver_name,
						void *cookie);

	status_t		(*update_interrupt_line) (
						uchar bus,
						uchar device,
						uchar function,
						uchar newInterruptLineValue);

	status_t		(*find_pci_extended_capability) (
						uint8	bus,
						uint8	device,
						uint8	function,
						uint16	cap_id,
						uint16	*offset
					);

	status_t		(*get_powerstate)(uint8 bus, uint8 device, uint8 function, uint8* state);
	status_t		(*set_powerstate)(uint8 bus, uint8 device, uint8 function, uint8 newState);

	uint32			(*get_msi_count)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function);		/* function # in device */

	status_t		(*configure_msi)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function,			/* function # in device */
						uint32 count,			/* count of vectors desired */
						uint32 *startVector);	/* first configured vector */
	status_t		(*unconfigure_msi)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function);		/* function # in device */

	status_t		(*enable_msi)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function);		/* function # in device */
	status_t		(*disable_msi)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function);		/* function # in device */

	uint32			(*get_msix_count)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function);		/* function # in device */

	status_t		(*configure_msix)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function,			/* function # in device */
						uint32 count,			/* count of vectors desired */
						uint32 *startVector);	/* first configured vector */
	status_t		(*enable_msix)(
						uint8 bus,				/* bus number */
						uint8 device,			/* device # on bus */
						uint8 function);		/* function # in device */
};

#define	B_PCI_MODULE_NAME		"bus_managers/pci/v1"

/* ---
	offsets in PCI configuration space to the elements of the predefined
	header common to all header types
--- */

#define PCI_vendor_id			0x00		/* (2 byte) vendor id */
#define PCI_device_id			0x02		/* (2 byte) device id */
#define PCI_command				0x04		/* (2 byte) command */
#define PCI_status				0x06		/* (2 byte) status */
#define PCI_revision			0x08		/* (1 byte) revision id */
#define PCI_class_api			0x09		/* (1 byte) specific register interface type */
#define PCI_class_sub			0x0a		/* (1 byte) specific device function */
#define PCI_class_base			0x0b		/* (1 byte) device type (display vs network, etc) */
#define PCI_line_size			0x0c		/* (1 byte) cache line size in 32 bit words */
#define PCI_latency				0x0d		/* (1 byte) latency timer */
#define PCI_header_type			0x0e		/* (1 byte) header type */
#define PCI_bist				0x0f		/* (1 byte) built-in self-test */

#define PCI_extended_capability 0x100		/* (4 bytes) extended capability */


/* ---
	offsets in PCI configuration space to the elements of the predefined
	header common to header types 0x00 and 0x01
--- */
#define PCI_base_registers		0x10		/* base registers (size varies) */
#define PCI_interrupt_line		0x3c		/* (1 byte) interrupt line */
#define PCI_interrupt_pin		0x3d		/* (1 byte) interrupt pin */



/* ---
	offsets in PCI configuration space to the elements of header type 0x00
--- */

#define PCI_cardbus_cis			0x28		/* (4 bytes) CardBus CIS (Card Information Structure) pointer (see PCMCIA v2.10 Spec) */
#define PCI_subsystem_vendor_id	0x2c		/* (2 bytes) subsystem (add-in card) vendor id */
#define PCI_subsystem_id		0x2e		/* (2 bytes) subsystem (add-in card) id */
#define PCI_rom_base			0x30		/* (4 bytes) expansion rom base address */
#define PCI_capabilities_ptr    0x34        /* (1 byte) pointer to the start of the capabilities list */
#define PCI_min_grant			0x3e		/* (1 byte) burst period @ 33 Mhz */
#define PCI_max_latency			0x3f		/* (1 byte) how often PCI access needed */


/* ---
	offsets in PCI configuration space to the elements of header type 0x01 (PCI-to-PCI bridge)
--- */

#define PCI_primary_bus								0x18 /* (1 byte) */
#define PCI_secondary_bus							0x19 /* (1 byte) */
#define PCI_subordinate_bus							0x1A /* (1 byte) */
#define PCI_secondary_latency						0x1B /* (1 byte) latency of secondary bus */
#define PCI_io_base									0x1C /* (1 byte) io base address register for 2ndry bus*/
#define PCI_io_limit								0x1D /* (1 byte) */
#define PCI_secondary_status						0x1E /* (2 bytes) */
#define PCI_memory_base								0x20 /* (2 bytes) */
#define PCI_memory_limit							0x22 /* (2 bytes) */
#define PCI_prefetchable_memory_base				0x24 /* (2 bytes) */
#define PCI_prefetchable_memory_limit				0x26 /* (2 bytes) */
#define PCI_prefetchable_memory_base_upper32		0x28
#define PCI_prefetchable_memory_limit_upper32		0x2C
#define PCI_io_base_upper16							0x30 /* (2 bytes) */
#define PCI_io_limit_upper16						0x32 /* (2 bytes) */
#define PCI_sub_vendor_id_1                         0x34 /* (2 bytes) */
#define PCI_sub_device_id_1                         0x36 /* (2 bytes) */
#define PCI_bridge_rom_base							0x38
#define PCI_bridge_control							0x3E /* (2 bytes) */


/* PCI type 2 header offsets */
#define PCI_capabilities_ptr_2                      0x14 /* (1 byte) */
#define PCI_secondary_status_2                      0x16 /* (2 bytes) */
#define PCI_primary_bus_2							0x18 /* (1 byte) */
#define PCI_secondary_bus_2							0x19 /* (1 byte) */
#define PCI_subordinate_bus_2						0x1A /* (1 byte) */
#define PCI_secondary_latency_2                     0x1B /* (1 byte) latency of secondary bus */
#define PCI_memory_base0_2                          0x1C /* (4 bytes) */
#define PCI_memory_limit0_2                         0x20 /* (4 bytes) */
#define PCI_memory_base1_2                          0x24 /* (4 bytes) */
#define PCI_memory_limit1_2                         0x28 /* (4 bytes) */
#define PCI_io_base0_2                              0x2c /* (4 bytes) */
#define PCI_io_limit0_2                             0x30 /* (4 bytes) */
#define PCI_io_base1_2                              0x34 /* (4 bytes) */
#define PCI_io_limit1_2                             0x38 /* (4 bytes) */
#define PCI_bridge_control_2                        0x3E /* (2 bytes) */

#define PCI_sub_vendor_id_2                         0x40 /* (2 bytes) */
#define PCI_sub_device_id_2                         0x42 /* (2 bytes) */

#define PCI_card_interface_2                        0x44 /* ?? */

/* ---
	values for the class_base field in the common header
--- */

#define PCI_early					0x00	/* built before class codes defined */
#define PCI_mass_storage			0x01	/* mass storage_controller */
#define PCI_network					0x02	/* network controller */
#define PCI_display					0x03	/* display controller */
#define PCI_multimedia				0x04	/* multimedia device */
#define PCI_memory					0x05	/* memory controller */
#define PCI_bridge					0x06	/* bridge controller */
#define PCI_simple_communications	0x07	/* simple communications controller */
#define PCI_base_peripheral			0x08	/* base system peripherals */
#define PCI_input					0x09	/* input devices */
#define PCI_docking_station			0x0a	/* docking stations */
#define PCI_processor				0x0b	/* processors */
#define PCI_serial_bus				0x0c	/* serial bus controllers */
#define PCI_wireless				0x0d	/* wireless controllers */
#define PCI_intelligent_io			0x0e
#define PCI_satellite_communications 0x0f
#define PCI_encryption_decryption	0x10
#define PCI_data_acquisition		0x11	/* data acquisition and
												signal processing controllers */
#define PCI_processing_accelerator	0x12	/* processing accelerators */
#define PCI_nonessential_function	0x13	/* non-essential instrumentation
												function  */

#define PCI_undefined				0xFF	/* not in any defined class */


/* ---
	values for the class_sub field for class_base = 0x00 (built before
	class codes were defined)
--- */

#define PCI_early_not_vga	0x00			/* all except vga */
#define PCI_early_vga		0x01			/* vga devices */


/* ---
	values for the class_sub field for class_base = 0x01 (mass storage)
--- */

#define PCI_scsi			0x00			/* SCSI controller */
#define PCI_ide				0x01			/* IDE controller */
#define PCI_floppy			0x02			/* floppy disk controller */
#define PCI_ipi				0x03			/* IPI bus controller */
#define PCI_raid			0x04			/* RAID controller */
#define PCI_ata				0x05			/* ATA controller with ADMA interface */
#define PCI_sata			0x06			/* Serial ATA controller */
#define PCI_sas				0x07			/* Serial Attached SCSI controller */
#define PCI_nvm				0x08			/* NVM Express controller */
#define PCI_ufs				0x09			/* Universal Flash Storage controller */
#define PCI_mass_storage_other 0x80			/* other mass storage controller */

/* ---
	bit mask of the class_api field for
		class_base	= 0x01 (mass storage)
		class_sub	= 0x01 (IDE controller)
--- */

#define PCI_ide_primary_native		0x01	/* primary channel is in native mode */
#define PCI_ide_primary_fixed		0x02	/* primary channel can be switched to native mode */
#define PCI_ide_secondary_native	0x04	/* secondary channel is in native mode */
#define PCI_ide_secondary_fixed		0x08	/* secondary channel can be switched to native mode */
#define PCI_ide_master				0x80	/* master device */

/* ---
	values of the class_api field for
		class_base	= 0x01 (mass storage)
		class_sub	= 0x06 (Serial ATA controller)
--- */

#define PCI_sata_other			0x00	/* vendor specific interface */
#define PCI_sata_ahci			0x01	/* AHCI interface */

/* ---
	values of the class_api field for
		class_base	= 0x01 (mass storage)
		class_sub	= 0x08 (NVM Express controller)
--- */

#define PCI_nvm_other			0x00	/* vendor specific interface */
#define PCI_nvm_hci				0x01	/* NVMHCI interface 1.0 */
#define PCI_nvm_hci_enterprise	0x02	/* NVMHCI enterprise */

/* ---
	values of the class_api field for
		class_base	= 0x01 (mass storage)
		class_sub	= 0x09 (Universal Flash Storage controller)
--- */

#define PCI_ufs_other			0x00	/* vendor specific interface */
#define PCI_ufs_hci				0x01	/* UFSHCI interface */

/* ---
	values for the class_sub field for class_base = 0x02 (network)
--- */

#define PCI_ethernet		0x00			/* Ethernet controller */
#define PCI_token_ring		0x01			/* Token Ring controller */
#define PCI_fddi			0x02			/* FDDI controller */
#define PCI_atm				0x03			/* ATM controller */
#define PCI_isdn            0x04            /* ISDN controller */
#define PCI_worldfip		0x05            /* WorldFip controller */
#define PCI_picmg			0x06            /* PICMG controller */
#define PCI_network_infiniband	0x07		/* InfiniBand controller */
#define PCI_hfc				0x08			/* Host fabric controller */
#define PCI_network_other	0x80			/* other network controller */


/* ---
	values for the class_sub field for class_base = 0x03 (display)
--- */

#define PCI_vga				0x00			/* VGA controller */
#define PCI_xga				0x01			/* XGA controller */
#define PCI_3d              0x02            /* 3d controller */
#define PCI_display_other	0x80			/* other display controller */


/* ---
	values for the class_sub field for class_base = 0x04 (multimedia device)
--- */

#define PCI_video				0x00		/* video */
#define PCI_audio				0x01		/* audio */
#define PCI_telephony			0x02		/* computer telephony device */
#define PCI_hd_audio			0x03		/* HD audio */
#define PCI_multimedia_other	0x80		/* other multimedia device */


/* ---
	values of the class_api field for
		class_base	= 0x04 (multimedia device)
		class_sub	= 0x03 (HD audio)
--- */

#define PCI_hd_audio_vendor	0x80	/* with additional vendor specific extensions */

/* ---
	values for the class_sub field for class_base = 0x05 (memory)
--- */

#define PCI_ram				0x00			/* RAM */
#define PCI_flash			0x01			/* flash */
#define PCI_memory_other	0x80			/* other memory controller */


/* ---
	values for the class_sub field for class_base = 0x06 (bridge)
--- */

#define PCI_host			0x00			/* host bridge */
#define PCI_isa				0x01			/* ISA bridge */
#define PCI_eisa			0x02			/* EISA bridge */
#define PCI_microchannel	0x03			/* MicroChannel bridge */
#define PCI_pci				0x04			/* PCI-to-PCI bridge */
#define PCI_pcmcia			0x05			/* PCMCIA bridge */
#define PCI_nubus			0x06			/* NuBus bridge */
#define PCI_cardbus			0x07			/* CardBus bridge */
#define PCI_raceway			0x08			/* RACEway bridge */
#define PCI_bridge_transparent	0x09			/* PCI transparent */
#define PCI_bridge_infiniband	0x0a			/* Infiniband */
#define PCI_bridge_as_pci	0x0b	/* Advanced Switching to PCI host bridge */
#define PCI_bridge_other		0x80			/* other bridge device */

/* ---
	values of the class_api field for
		class_base	= 0x06 (bridge), and
		class_sub	= 0x0b (Advanced Switching to PCI host bridge)
--- */

#define PCI_bridge_as_pci_asi_sig	0x01	/* ASI-SIG Defined Portal Interface */

/* ---
	values for the class_sub field for class_base = 0x07 (simple
	communications controllers)
--- */

#define PCI_serial						0x00	/* serial port controller */
#define PCI_parallel					0x01	/* parallel port */
#define PCI_multiport_serial            0x02    /* multiport serial controller */
#define PCI_modem                       0x03    /* modem */
#define PCI_gpib						0x04    /* GPIB controller */
#define PCI_smart_card					0x05	/* Smard Card controller */
#define PCI_simple_communications_other	0x80	/* other communications device */

/* ---
	values of the class_api field for
		class_base	= 0x07 (simple communications), and
		class_sub	= 0x00 (serial port controller)
--- */

#define PCI_serial_xt		0x00			/* XT-compatible serial controller */
#define PCI_serial_16450	0x01			/* 16450-compatible serial controller */
#define PCI_serial_16550	0x02			/* 16550-compatible serial controller */


/* ---
	values of the class_api field for
		class_base	= 0x07 (simple communications), and
		class_sub	= 0x01 (parallel port)
--- */

#define PCI_parallel_simple			0x00	/* simple (output-only) parallel port */
#define PCI_parallel_bidirectional	0x01	/* bidirectional parallel port */
#define PCI_parallel_ecp			0x02	/* ECP 1.x compliant parallel port */


/* ---
	values for the class_sub field for class_base = 0x08 (generic
	system peripherals)
--- */

#define PCI_pic						0x00	/* peripheral interrupt controller */
#define PCI_dma						0x01	/* dma controller */
#define PCI_timer					0x02	/* timers */
#define PCI_rtc						0x03	/* real time clock */
#define PCI_generic_hot_plug        0x04    /* generic PCI hot-plug controller */
#define PCI_sd_host					0x05	/* SD Host controller */
#define PCI_iommu					0x06	/* IOMMU */
#define PCI_rcec					0x07	/* Root Complex Event Collector */
#define PCI_system_peripheral_other	0x80	/* other generic system peripheral */

/* ---
	values of the class_api field for
		class_base	= 0x08 (generic system peripherals)
		class_sub	= 0x00 (peripheral interrupt controller)
--- */

#define PCI_pic_8259			0x00	/* generic 8259 */
#define PCI_pic_isa				0x01	/* ISA pic */
#define PCI_pic_eisa			0x02	/* EISA pic */

/* ---
	values of the class_api field for
		class_base	= 0x08 (generic system peripherals)
		class_sub	= 0x01 (dma controller)
--- */

#define PCI_dma_8237			0x00	/* generic 8237 */
#define PCI_dma_isa				0x01	/* ISA dma */
#define PCI_dma_eisa			0x02	/* EISA dma */

/* ---
	values of the class_api field for
		class_base	= 0x08 (generic system peripherals)
		class_sub	= 0x02 (timer)
--- */

#define PCI_timer_8254			0x00	/* generic 8254 */
#define PCI_timer_isa			0x01	/* ISA timer */
#define PCI_timer_eisa			0x02	/* EISA timers (2 timers) */


/* ---
	values of the class_api field for
		class_base	= 0x08 (generic system peripherals)
		class_sub	= 0x03 (real time clock
--- */

#define PCI_rtc_generic			0x00	/* generic real time clock */
#define PCI_rtc_isa				0x01	/* ISA real time clock */


/* ---
	values for the class_sub field for class_base = 0x09 (input devices)
--- */

#define PCI_keyboard			0x00	/* keyboard controller */
#define PCI_pen					0x01	/* pen */
#define PCI_mouse				0x02	/* mouse controller */
#define PCI_scanner             0x03    /* scanner controller */
#define PCI_gameport            0x04    /* gameport controller */
#define PCI_input_other			0x80	/* other input controller */


/* ---
	values for the class_sub field for class_base = 0x0a (docking stations)
--- */

#define PCI_docking_generic		0x00	/* generic docking station */
#define PCI_docking_other		0x80	/* other docking stations */

/* ---
	values for the class_sub field for class_base = 0x0b (processor)
--- */

#define PCI_386					0x00	/* 386 */
#define PCI_486					0x01	/* 486 */
#define PCI_pentium				0x02	/* Pentium */
#define PCI_alpha				0x10	/* Alpha */
#define PCI_PowerPC				0x20	/* PowerPC */
#define PCI_mips                0x30    /* MIPS */
#define PCI_coprocessor			0x40	/* co-processor */

/* ---
	values for the class_sub field for class_base = 0x0c (serial bus
	controller)
--- */

#define PCI_firewire			0x00	/* FireWire (IEEE 1394) */
#define PCI_access				0x01	/* ACCESS bus */
#define PCI_ssa					0x02	/* SSA */
#define PCI_usb					0x03	/* Universal Serial Bus */
#define PCI_fibre_channel		0x04	/* Fibre channel */
#define PCI_smbus			0x05
#define PCI_infiniband			0x06
#define PCI_ipmi			0x07
#define PCI_sercos			0x08
#define PCI_canbus			0x09
#define PCI_mipi_i3c		0x0a	/* MIPI I3C Host Controller Interface */

/* ---
	values of the class_api field for
		class_base	= 0x0c ( serial bus controller )
		class_sub	= 0x03 ( Universal Serial Bus  )
--- */

#define PCI_usb_uhci			0x00	/* Universal Host Controller Interface */
#define PCI_usb_ohci			0x10	/* Open Host Controller Interface */
#define PCI_usb_ehci			0x20	/* Enhanced Host Controller Interface */
#define PCI_usb_xhci			0x30	/* Extensible Host Controller Interface */
#define PCI_usb_usb4			0x40	/* USB4 Host Interface */

/* ---
	values for the class_sub field for class_base = 0x0d (wireless controller)
--- */
#define PCI_wireless_irda			0x00
#define PCI_wireless_consumer_ir	0x01
#define PCI_wireless_rf				0x10
#define PCI_wireless_bluetooth		0x11
#define PCI_wireless_broadband		0x12
#define PCI_wireless_80211A			0x20
#define PCI_wireless_80211B			0x21
#define PCI_wireless_cellular		0x40
#define PCI_wireless_cellular_ethernet	0x41
#define PCI_wireless_other			0x80

/* ---
	values for the class_sub field for class_base = 0x10 (encryption decryption)
--- */
#define PCI_encryption_decryption_network_computing		0x00
#define PCI_encryption_decryption_entertainment			0x10
#define PCI_encryption_decryption_other					0x80

/* ---
	values for the class_sub field for class_base = 0x11 (data acquisition)
--- */
#define PCI_data_acquisition_dpio	0x00
#define PCI_data_acquisition_performance_counters		0x01
#define PCI_data_acquisition_communication_synchroniser	0x10
#define PCI_data_acquisition_management					0x20
#define PCI_data_acquisition_other						0x80

/* ---
	masks for command register bits
--- */

#define PCI_command_io				0x001		/* 1/0 i/o space en/disabled */
#define PCI_command_memory			0x002		/* 1/0 memory space en/disabled */
#define PCI_command_master			0x004		/* 1/0 pci master en/disabled */
#define PCI_command_special			0x008		/* 1/0 pci special cycles en/disabled */
#define PCI_command_mwi				0x010		/* 1/0 memory write & invalidate en/disabled */
#define PCI_command_vga_snoop		0x020		/* 1/0 vga pallette snoop en/disabled */
#define PCI_command_parity			0x040		/* 1/0 parity check en/disabled */
#define PCI_command_address_step	0x080		/* 1/0 address stepping en/disabled */
#define PCI_command_serr			0x100		/* 1/0 SERR# en/disabled */
#define PCI_command_fastback		0x200		/* 1/0 fast back-to-back en/disabled */
#define PCI_command_int_disable		0x400		/* 1/0 interrupt generation dis/enabled */


/* ---
	masks for status register bits
--- */

#define PCI_status_capabilities             0x0010  /* capabilities list */
#define PCI_status_66_MHz_capable			0x0020	/* 66 Mhz capable */
#define PCI_status_udf_supported			0x0040	/* user-definable-features (udf) supported */
#define PCI_status_fastback					0x0080	/* fast back-to-back capable */
#define PCI_status_parity_signalled		    0x0100	/* parity error signalled */
#define PCI_status_devsel					0x0600	/* devsel timing (see below) */
#define PCI_status_target_abort_signalled	0x0800	/* signaled a target abort */
#define PCI_status_target_abort_received	0x1000	/* received a target abort */
#define PCI_status_master_abort_received	0x2000	/* received a master abort */
#define PCI_status_serr_signalled			0x4000	/* signalled SERR# */
#define PCI_status_parity_error_detected	0x8000	/* parity error detected */


/* ---
	masks for devsel field in status register
--- */

#define PCI_status_devsel_fast		0x0000		/* fast */
#define PCI_status_devsel_medium	0x0200		/* medium */
#define PCI_status_devsel_slow		0x0400		/* slow */


/* ---
	masks for header type register
--- */

#define PCI_header_type_mask	0x7F		/* header type field */
#define PCI_multifunction		0x80		/* multifunction device flag */


/** types of PCI header */

#define PCI_header_type_generic				0x00
#define PCI_header_type_PCI_to_PCI_bridge	0x01
#define PCI_header_type_cardbus             0x02


/* ---
	masks for built in self test (bist) register bits
--- */

#define PCI_bist_code			0x0F		/* self-test completion code, 0 = success */
#define PCI_bist_start			0x40		/* 1 = start self-test */
#define PCI_bist_capable		0x80		/* 1 = self-test capable */


/** masks for flags in the various base address registers */

#define PCI_address_space		0x01		/* 0 = memory space, 1 = i/o space */
#define PCI_register_start      0x10
#define PCI_register_end        0x24
#define PCI_register_ppb_end    0x18
#define PCI_register_pcb_end    0x14

/** masks for flags in memory space base address registers */

#define PCI_address_type_32			0x00	/* locate anywhere in 32 bit space */
#define PCI_address_type_32_low		0x02	/* locate below 1 Meg */
#define PCI_address_type_64			0x04	/* locate anywhere in 64 bit space */
#define PCI_address_type			0x06	/* type (see below) */
#define PCI_address_prefetchable	0x08	/* 1 if prefetchable (see PCI spec) */

#define PCI_address_memory_32_mask	0xFFFFFFF0	/* mask to get 32bit memory space base address */


/* ---
	masks for flags in i/o space base address registers
--- */

#define PCI_address_io_mask		0xFFFFFFFC	/* mask to get i/o space base address */

#define PCI_range_memory_mask	0xFFFFFFF0	/* mask to get memory ranges */


/* ---
	masks for flags in expansion rom base address registers
--- */

#define PCI_rom_enable			0x00000001	/* 1 expansion rom decode enabled */
#define PCI_rom_shadow			0x00000010	/* 2 rom copied at shadow (C0000) */
#define PCI_rom_copy			0x00000100	/* 4 rom is allocated copy */
#define PCI_rom_bios			0x00001000	/* 8 rom is bios copy */
#define PCI_rom_address_mask	0xFFFFF800	/* mask to get expansion rom addr */

/** PCI interrupt pin values */
#define PCI_pin_mask            0x07
#define PCI_pin_none            0x00
#define PCI_pin_a               0x01
#define PCI_pin_b               0x02
#define PCI_pin_c               0x03
#define PCI_pin_d               0x04
#define PCI_pin_max             0x04

/** PCI bridge control register bits */
#define PCI_bridge_parity_error_response	0x0001	/* 1/0 Parity Error Response */
#define PCI_bridge_serr						0x0002	/* 1/0 SERR# en/disabled */
#define PCI_bridge_isa						0x0004	/* 1/0 ISA en/disabled */
#define PCI_bridge_vga						0x0008	/* 1/0 VGA en/disabled */
#define PCI_bridge_master_abort				0x0020	/* 1/0 Master Abort mode */
#define PCI_bridge_secondary_bus_reset		0x0040	/* 1/0 Secondary bus reset */
#define PCI_bridge_secondary_bus_fastback	0x0080	/* 1/0 fast back-to-back en/disabled */
#define PCI_bridge_primary_discard_timeout	0x0100	/* 1/0 primary discard timeout */
#define PCI_bridge_secondary_discard_timeout	0x0200	/* 1/0 secondary discard timeout */
#define PCI_bridge_discard_timer_status		0x0400	/* 1/0 discard timer status */
#define PCI_bridge_discard_timer_serr		0x0800	/* 1/0 discard timer serr */

/** PCI Capability Codes */
#define PCI_cap_id_reserved	0x00
#define PCI_cap_id_pm		0x01      /* Power management */
#define PCI_cap_id_agp		0x02      /* AGP */
#define PCI_cap_id_vpd		0x03      /* Vital product data */
#define PCI_cap_id_slotid	0x04      /* Slot ID */
#define PCI_cap_id_msi		0x05      /* Message signalled interrupt */
#define PCI_cap_id_chswp	0x06      /* Compact PCI HotSwap */
#define PCI_cap_id_pcix		0x07      /* PCI-X */
#define PCI_cap_id_ht		0x08	  /* HyperTransport */
#define PCI_cap_id_vendspec	0x09
#define PCI_cap_id_debugport	0x0a
#define PCI_cap_id_cpci_rsrcctl 0x0b
#define PCI_cap_id_hotplug      0x0c
#define PCI_cap_id_subvendor	0x0d
#define PCI_cap_id_agp8x	0x0e
#define PCI_cap_id_secure_dev	0x0f
#define PCI_cap_id_pcie		0x10	/* PCIe (PCI express) */
#define PCI_cap_id_msix		0x11	/* MSI-X */
#define PCI_cap_id_sata		0x12	/* Serial ATA Capability */
#define PCI_cap_id_pciaf	0x13	/* PCI Advanced Features */
#define PCI_cap_id_ea		0x14	/* Extended Allocation */
#define PCI_cap_id_fpb		0x15	/* Flattening Portal Bridge */

/** PCI Extended Capabilities */
#define PCI_extcap_id(x)		(x & 0x0000ffff)
#define PCI_extcap_version(x)	((x & 0x000f0000) >> 16)
#define PCI_extcap_next_ptr(x)	((x & 0xfff00000) >> 20)

#define PCI_extcap_id_aer			0x0001	/* Advanced Error Reporting */
#define PCI_extcap_id_vc			0x0002	/* Virtual Channel */
#define PCI_extcap_id_serial		0x0003	/* Serial Number */
#define PCI_extcap_id_power_budget	0x0004	/* Power Budgeting */
#define PCI_extcap_id_rcl_decl		0x0005	/* Root Complex Link Declaration */
#define PCI_extcap_id_rcil_ctl		0x0006	/* Root Complex Internal Link Control */
#define PCI_extcap_id_rcec_assoc	0x0007	/* Root Complex Event Collector Association */
#define PCI_extcap_id_mfvc			0x0008	/* MultiFunction Virtual Channel */
#define PCI_extcap_id_vc2			0x0009	/* Virtual Channel 2 */
#define PCI_extcap_id_rcrb_header	0x000a	/* RCRB Header */
#define PCI_extcap_id_vendor		0x000b	/* Vendor Unique */
#define PCI_extcap_id_acs			0x000d	/* Access Control Services */
#define PCI_extcap_id_ari			0x000e	/* Alternative Routing Id Interpretation */
#define PCI_extcap_id_ats			0x000f	/* Address Translation Services */
#define PCI_extcap_id_srio_virtual	0x0010	/* Single Root I/O Virtualization */
#define PCI_extcap_id_mrio_virtual	0x0011	/* Multiple Root I/O Virtual */
#define PCI_extcap_id_multicast		0x0012	/* Multicast */
#define PCI_extcap_id_page_request	0x0013	/* Page Request */
#define PCI_extcap_id_amd			0x0014	/* AMD Reserved */
#define PCI_extcap_id_resizable_bar	0x0015	/* Resizable Bar */
#define PCI_extcap_id_dyn_power_alloc	0x0016	/* Dynamic Power Allocation */
#define PCI_extcap_id_tph_requester	0x0017	/* TPH Requester */
#define PCI_extcap_id_latency_tolerance	0x0018	/* Latency Tolerance Reporting */
#define PCI_extcap_id_2ndpcie		0x0019	/* Secondary PCIe */
#define PCI_extcap_id_pmux			0x001a	/* Protocol Multiplexing */
#define PCI_extcap_id_pasid			0x001b	/* Process Address Space Id */
#define PCI_extcap_id_ln_requester	0x001c	/* LN Requester */
#define PCI_extcap_id_dpc			0x001d	/* Downstream Porto Containment */
#define PCI_extcap_id_l1pm			0x001e	/* L1 Power Management Substates */
#define PCI_extcap_id_ptm			0x001f	/* Precision Time Measurement */
#define PCI_extcap_id_m_pcie		0x0020	/* PCIe over M-PHY */
#define PCI_extcap_id_frs			0x0021	/* FRS Queuing */
#define PCI_extcap_id_rtr			0x0022	/* Readiness Time Reporting */
#define PCI_extcap_id_dvsec			0x0023	/* Designated Vendor-Specific */
#define PCI_extcap_id_vf_resizable_bar	0x0024	/* VF Resizable BAR */
#define PCI_extcap_id_datalink		0x0025	/* Data Link Feature */
#define PCI_extcap_id_16gt			0x0026	/* Physical Layer 16.0 GT/s */
#define PCI_extcap_id_lmr			0x0027	/* Lane Marging at the Receiver */
#define PCI_extcap_id_hierarchy_id	0x0028	/* Hierarchy ID */
#define PCI_extcap_id_npem			0x0029	/* Native PCIe Enclosure Management */
#define PCI_extcap_id_pl32			0x002a	/* Physical Layer 32.0 GT/s */
#define PCI_extcap_id_ap			0x002b	/* Alternate Protocol */
#define PCI_extcap_id_sfi			0x002c	/* System Firmware Intermediary */
#define PCI_extcap_id_sf			0x002d	/* Shadow Functions */
#define PCI_extcap_id_doe			0x002e	/* Data Object Exchange */

/** Power Management Control Status Register settings */
#define PCI_pm_mask             0x03
#define PCI_pm_ctrl             0x02
#define PCI_pm_d1supp           0x0200
#define PCI_pm_d2supp           0x0400
#define PCI_pm_status           0x04
#define PCI_pm_state_d0         0x00
#define PCI_pm_state_d1         0x01
#define PCI_pm_state_d2         0x02
#define PCI_pm_state_d3         0x03

/** MSI registers **/
#define PCI_msi_control			0x02
#define PCI_msi_address			0x04
#define PCI_msi_address_high	0x08
#define PCI_msi_data			0x08
#define PCI_msi_data_64bit		0x0c
#define PCI_msi_mask			0x10
#define PCI_msi_pending			0x14

/** MSI control register values **/
#define PCI_msi_control_enable		0x0001
#define PCI_msi_control_vector		0x0100
#define PCI_msi_control_64bit		0x0080
#define PCI_msi_control_mme_mask	0x0070
#define PCI_msi_control_mme_1		0x0000
#define PCI_msi_control_mme_2		0x0010
#define PCI_msi_control_mme_4		0x0020
#define PCI_msi_control_mme_8		0x0030
#define PCI_msi_control_mme_16		0x0040
#define PCI_msi_control_mme_32		0x0050
#define PCI_msi_control_mmc_mask	0x000e
#define PCI_msi_control_mmc_1		0x0000
#define PCI_msi_control_mmc_2		0x0002
#define PCI_msi_control_mmc_4		0x0004
#define PCI_msi_control_mmc_8		0x0006
#define PCI_msi_control_mmc_16		0x0008
#define PCI_msi_control_mmc_32		0x000a

/** MSI-X registers **/
#define PCI_msix_control			0x02
#define PCI_msix_table				0x04
#define PCI_msix_pba				0x08

#define PCI_msix_control_table_size	0x07ff
#define PCI_msix_control_function_mask	0x4000
#define PCI_msix_control_enable		0x8000
#define PCI_msix_bir_mask			0x0007
#define PCI_msix_bir_0				0x10
#define PCI_msix_bir_1				0x14
#define PCI_msix_bir_2				0x18
#define PCI_msix_bir_3				0x1c
#define PCI_msix_bir_4				0x20
#define PCI_msix_bir_5				0x24
#define PCI_msix_offset_mask		0xfff8

#define PCI_msix_vctrl_mask			0x0001

/** HyperTransport registers **/
#define PCI_ht_command				0x02
#define PCI_ht_msi_address_low		0x04
#define PCI_ht_msi_address_high		0x08

#define PCI_ht_command_cap_mask_3_bits	0xe000
#define PCI_ht_command_cap_mask_5_bits	0xf800
#define PCI_ht_command_cap_slave	0x0000
#define PCI_ht_command_cap_host		0x2000
#define PCI_ht_command_cap_switch	0x4000
#define PCI_ht_command_cap_interrupt	0x8000
#define PCI_ht_command_cap_revision_id	0x8800
#define PCI_ht_command_cap_unit_id_clumping	0x9000
#define PCI_ht_command_cap_ext_config_space	0x9800
#define PCI_ht_command_cap_address_mapping	0xa000
#define PCI_ht_command_cap_msi_mapping	0xa800
#define PCI_ht_command_cap_direct_route	0xb000
#define PCI_ht_command_cap_vcset	0xb800
#define PCI_ht_command_cap_retry_mode	0xc000
#define PCI_ht_command_cap_x86_encoding	0xc800
#define PCI_ht_command_cap_gen3		0xd000
#define PCI_ht_command_cap_fle		0xd800
#define PCI_ht_command_cap_pm		0xe000
#define PCI_ht_command_cap_high_node_count	0xe800

#define PCI_ht_command_msi_enable	0x0001
#define PCI_ht_command_msi_fixed	0x0002

#ifdef __cplusplus
}
#endif

#endif	/* _PCI_H */
