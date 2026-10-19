/*
 * Copyright 2004-2008, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _DEVICE_MANAGER_H
#define _DEVICE_MANAGER_H


#include <TypeConstants.h>
#include <Drivers.h>
#include <module.h>


/* type of I/O resource */
enum {
	B_IO_MEMORY			= 1,
	B_IO_PORT			= 2,
	B_ISA_DMA_CHANNEL	= 3
};


/* I/O resource description */
typedef struct {
	uint32	type;
		/* type of I/O resource */

	uint32	base;
		/* I/O memory: first physical address (32 bit)
		 * I/O port: first port address (16 bit)
		 * ISA DMA channel: channel number (0-7)
		 */

	uint32	length;
		/* I/O memory: size of address range (32 bit)
		 * I/O port: size of port range (16 bit)
		 * ISA DMA channel: must be 1
		 */
} io_resource;


/* attribute of a device node */
typedef struct {
	const char		*name;
	type_code		type;			/* for supported types, see value */
	union {
		uint8		ui8;			/* B_UINT8_TYPE */
		uint16		ui16;			/* B_UINT16_TYPE */
		uint32		ui32;			/* B_UINT32_TYPE */
		uint64		ui64;			/* B_UINT64_TYPE */
		const char	*string;		/* B_STRING_TYPE */
		struct {					/* B_RAW_TYPE */
			const void *data;
			size_t	length;
		} raw;
	} value;
} device_attr;


typedef struct device_node device_node;
typedef struct driver_module_info driver_module_info;


/* interface of the device manager */

typedef struct device_manager_info {
	module_info info;

	status_t (*rescan_node)(device_node *node);

	status_t (*register_node)(device_node *parent, const char *moduleName,
					const device_attr *attrs, const io_resource *ioResources,
					device_node **_node);
	status_t (*unregister_node)(device_node *node);

	status_t (*get_driver)(device_node *node, driver_module_info **_module,
					void **_cookie);

	device_node *(*get_root_node)();
	status_t (*get_next_child_node)(device_node *parent,
					const device_attr *attrs, device_node **node);
	device_node *(*get_parent_node)(device_node *node);
	void (*put_node)(device_node *node);

	status_t (*publish_device)(device_node *node, const char *path,
					const char *deviceModuleName);
	status_t (*unpublish_device)(device_node *node, const char *path);

	int32 (*create_id)(const char *generator);
	status_t (*free_id)(const char *generator, uint32 id);

	status_t (*get_attr_uint8)(const device_node *node, const char *name,
					uint8 *value, bool recursive);
	status_t (*get_attr_uint16)(const device_node *node, const char *name,
					uint16 *value, bool recursive);
	status_t (*get_attr_uint32)(const device_node *node, const char *name,
					uint32 *value, bool recursive);
	status_t (*get_attr_uint64)(const device_node *node, const char *name,
					uint64 *value, bool recursive);
	status_t (*get_attr_string)(const device_node *node, const char *name,
					const char **_value, bool recursive);
	status_t (*get_attr_raw)(const device_node *node, const char *name,
					const void **_data, size_t *_size, bool recursive);

	status_t (*get_next_attr)(device_node *node, device_attr **_attr);

	status_t (*find_child_node)(device_node *parent,
					const device_attr *attrs, device_node **node);

} device_manager_info;


#define B_DEVICE_MANAGER_MODULE_NAME "system/device_manager/v1"


/* interface of device driver */

struct driver_module_info {
	module_info info;

	float (*supports_device)(device_node *parent);
	status_t (*register_device)(device_node *parent);

	status_t (*init_driver)(device_node *node, void **_driverCookie);
	void (*uninit_driver)(void *driverCookie);
	status_t (*register_child_devices)(void *driverCookie);
	status_t (*rescan_child_devices)(void *driverCookie);

	void (*device_removed)(void *driverCookie);
	status_t (*suspend)(void *driverCookie, int32 state);
	status_t (*resume)(void *driverCookie);
};


/* standard device node attributes */

#define B_DEVICE_PRETTY_NAME		"device/pretty name"		/* string */
#define B_DEVICE_MAPPING			"device/mapping"			/* string */
#define B_DEVICE_BUS				"device/bus"				/* string */
#define B_DEVICE_FIXED_CHILD		"device/fixed child"		/* string */
#define B_DEVICE_FLAGS				"device/flags"				/* uint32 */

#define B_DEVICE_VENDOR_ID			"device/vendor"				/* uint16 */
#define B_DEVICE_ID					"device/id"					/* uint16 */
#define B_DEVICE_TYPE				"device/type"
	/* uint16, PCI base class */
#define B_DEVICE_SUB_TYPE			"device/subtype"
	/* uint16, PCI sub type */
#define B_DEVICE_INTERFACE			"device/interface"
	/* uint16, PCI class API */

#define B_DEVICE_UNIQUE_ID			"device/unique id"			/* string */

#define B_DEVICE_PUBLISHED_PATH		"device/published path"

/* device flags */
#define B_FIND_CHILD_ON_DEMAND		0x01
#define B_FIND_MULTIPLE_CHILDREN	0x02
#define B_KEEP_DRIVER_LOADED		0x04

/* DMA attributes */
#define B_DMA_LOW_ADDRESS			"dma/low_address"
#define B_DMA_HIGH_ADDRESS			"dma/high_address"
#define B_DMA_ALIGNMENT				"dma/alignment"
#define B_DMA_BOUNDARY				"dma/boundary"
#define B_DMA_MAX_TRANSFER_BLOCKS	"dma/max_transfer_blocks"
#define B_DMA_MAX_SEGMENT_BLOCKS	"dma/max_segment_blocks"
#define B_DMA_MAX_SEGMENT_COUNT		"dma/max_segment_count"

/* interface of device */

typedef struct IORequest io_request;

struct device_module_info {
	module_info info;

	status_t (*init_device)(void *driverCookie, void **_deviceCookie);
	void (*uninit_device)(void *deviceCookie);
	void (*device_removed)(void *deviceCookie);

	status_t (*open)(void *deviceCookie, const char *path, int openMode,
					void **_cookie);
	status_t (*close)(void *cookie);
	status_t (*free)(void *cookie);
	status_t (*read)(void *cookie, off_t pos, void *buffer, size_t *_length);
	status_t (*write)(void *cookie, off_t pos, const void *buffer,
					size_t *_length);
	status_t (*io)(void *cookie, io_request *request);
	status_t (*control)(void *cookie, uint32 op, void *buffer, size_t length);
	status_t (*select)(void *cookie, uint8 event, selectsync *sync);
	status_t (*deselect)(void *cookie, uint8 event, selectsync *sync);
};


#endif	/* _DEVICE_MANAGER_H */
