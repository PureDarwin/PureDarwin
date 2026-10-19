/*
 * Copyright 2005-2008, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License
 */
#ifndef _ACPI_H
#define _ACPI_H


#include <device_manager.h>
#include <KernelExport.h>


typedef struct acpi_module_info acpi_module_info;
typedef union acpi_object_type acpi_object_type;

#define B_ACPI_MODULE_NAME "bus_managers/acpi/v1"

typedef phys_addr_t acpi_physical_address;
typedef phys_addr_t acpi_io_address;
typedef size_t acpi_size;

/* Actually a ptr to a NS Node */
typedef void *				acpi_handle;

#ifndef __ACTYPES_H__

/* Notify types */

#define ACPI_SYSTEM_NOTIFY				0x1
#define ACPI_DEVICE_NOTIFY				0x2
#define ACPI_ALL_NOTIFY					(ACPI_SYSTEM_NOTIFY | ACPI_DEVICE_NOTIFY)
#define ACPI_MAX_NOTIFY_HANDLER_TYPE	0x3

#define ACPI_MAX_SYS_NOTIFY				0x7F

/* Address Space (Operation Region) Types */

enum {
	ACPI_ADR_SPACE_SYSTEM_MEMORY	= 0,
	ACPI_ADR_SPACE_SYSTEM_IO		= 1,
	ACPI_ADR_SPACE_PCI_CONFIG		= 2,
	ACPI_ADR_SPACE_EC				= 3,
	ACPI_ADR_SPACE_SMBUS			= 4,
	ACPI_ADR_SPACE_CMOS 			= 5,
	ACPI_ADR_SPACE_PCI_BAR_TARGET	= 6,
	ACPI_ADR_SPACE_IPMI				= 7,
	ACPI_ADR_SPACE_DATA_TABLE		= 8,
	ACPI_ADR_SPACE_FIXED_HARDWARE	= 127
};

/* ACPI fixed event types */

enum {
	ACPI_EVENT_PMTIMER = 0,
	ACPI_EVENT_GLOBAL,
	ACPI_EVENT_POWER_BUTTON,
	ACPI_EVENT_SLEEP_BUTTON,
	ACPI_EVENT_RTC
};

/* ACPI Object Types */

enum {
	ACPI_TYPE_ANY = 0,
	ACPI_TYPE_INTEGER,
	ACPI_TYPE_STRING,
	ACPI_TYPE_BUFFER,
	ACPI_TYPE_PACKAGE,
	ACPI_TYPE_FIELD_UNIT,
	ACPI_TYPE_DEVICE,
	ACPI_TYPE_EVENT,
	ACPI_TYPE_METHOD,
	ACPI_TYPE_MUTEX,
	ACPI_TYPE_REGION,
	ACPI_TYPE_POWER,
	ACPI_TYPE_PROCESSOR,
	ACPI_TYPE_THERMAL,
	ACPI_TYPE_BUFFER_FIELD,
	ACPI_TYPE_DDB_HANDLE,
	ACPI_TYPE_DEBUG_OBJECT,
	ACPI_TYPE_LOCAL_REFERENCE = 0x14
};

/* ACPI control method arg type */

union acpi_object_type {
	uint32 object_type;
	struct {
		uint32 object_type;
		uint64 integer;
	} integer;
	struct {
		uint32 object_type;
		uint32 len;
		char *string; /* You have to allocate string space yourself */
	} string;
	struct {
		uint32 object_type;
		uint32 length;
		void *buffer;
	} buffer;
	struct {
		uint32 object_type;
		uint32 count;
		acpi_object_type *objects;
	} package;
	struct {
		uint32 object_type;
		uint32 actual_type;
		acpi_handle handle;
	} reference;
	struct {
		uint32 object_type;
		uint32 cpu_id;
		acpi_io_address pblk_address;
		uint32 pblk_length;
	} processor;
	struct {
		uint32 object_type;
		uint32 min_power_state;
		uint32 resource_order;
	} power_resource;
};


/*
 * List of objects, used as a parameter list for control method evaluation
 */
typedef struct acpi_objects {
	uint32				count;
	acpi_object_type	*pointer;
} acpi_objects;


typedef struct acpi_data {
	acpi_size			length;		/* Length in bytes of the buffer */
	void				*pointer;	/* pointer to buffer */
} acpi_data;


enum {
	ACPI_ALLOCATE_BUFFER = (acpi_size)-1,
	ACPI_ALLOCATE_LOCAL_BUFFER = (acpi_size)-2,
};


/*
 * acpi_status should return ACPI specific error codes, not BeOS ones.
 */
typedef uint32 acpi_status;

typedef struct acpi_resource acpi_resource;

#define ACPI_REENABLE_GPE	0x80


#endif	// __ACTYPES_H__


typedef uint32 (*acpi_event_handler)(void *context);
typedef uint32 (*acpi_gpe_handler) (acpi_handle GpeDevice, uint32 GpeNumber,
	void *context);

typedef acpi_status (*acpi_adr_space_handler)(uint32 function,
	acpi_physical_address address, uint32 bitWidth, int *value,
	void *handlerContext, void *regionContext);

typedef acpi_status (*acpi_adr_space_setup)(acpi_handle regionHandle,
	uint32 function, void *handlerContext, void **regionContext);

typedef void (*acpi_notify_handler)(acpi_handle device, uint32 value,
	void *context);

typedef acpi_status (*acpi_walk_resources_callback)(acpi_resource* resource,
	void* context);

typedef acpi_status (*acpi_walk_callback) (acpi_handle object, uint32 nestingLevel,
	void *context, void** returnValue);


struct acpi_module_info {
	module_info info;

	status_t	(*get_handle)(acpi_handle parent, const char *pathname,
					acpi_handle *retHandle);
	status_t 	(*get_name)(acpi_handle handle, uint32 nameType,
					char* returnedName, size_t bufferLength);

	/* Global Lock */

	status_t	(*acquire_global_lock)(uint16 timeout, uint32 *handle);
	status_t	(*release_global_lock)(uint32 handle);

	/* Notify Handler */

	status_t	(*install_notify_handler)(acpi_handle device,
					uint32 handlerType, acpi_notify_handler handler,
					void *context);
	status_t	(*remove_notify_handler)(acpi_handle device,
					uint32 handlerType, acpi_notify_handler handler);

	/* GPE Handler */

	status_t	(*update_all_gpes)();
	status_t	(*enable_gpe)(acpi_handle handle, uint32 gpeNumber);
	status_t	(*disable_gpe)(acpi_handle handle, uint32 gpeNumber);
	status_t	(*clear_gpe)(acpi_handle handle, uint32 gpeNumber);
	status_t	(*set_gpe)(acpi_handle handle, uint32 gpeNumber,
					uint8 action);
	status_t	(*finish_gpe)(acpi_handle handle, uint32 gpeNumber);
	status_t	(*install_gpe_handler)(acpi_handle handle, uint32 gpeNumber,
					uint32 type, acpi_gpe_handler handler, void *data);
	status_t	(*remove_gpe_handler)(acpi_handle handle, uint32 gpeNumber,
					acpi_gpe_handler address);

	/* Address Space Handler */

	status_t	(*install_address_space_handler)(acpi_handle handle,
					uint32 spaceId,
					acpi_adr_space_handler handler,
					acpi_adr_space_setup setup,	void *data);
	status_t	(*remove_address_space_handler)(acpi_handle handle,
					uint32 spaceId,
					acpi_adr_space_handler handler);

	/* Fixed Event Management */

	void		(*enable_fixed_event)(uint32 event);
	void		(*disable_fixed_event)(uint32 event);

	uint32		(*fixed_event_status) (uint32 event);
					/* Returns 1 if event set, 0 otherwise */
	void		(*reset_fixed_event) (uint32 event);

	status_t	(*install_fixed_event_handler)(uint32 event,
					acpi_event_handler handler, void *data);
	status_t	(*remove_fixed_event_handler)(uint32 event,
					acpi_event_handler handler);

	/* Namespace Access */

	status_t	(*get_next_entry)(uint32 objectType, const char *base,
					char *result, size_t length, void **_counter);
	status_t	(*get_next_object)(uint32 objectType, acpi_handle parent,
					acpi_handle* currentChild);
	status_t	(*walk_namespace)(acpi_handle busDeviceHandle,
					uint32 objectType, uint32 maxDepth,
					acpi_walk_callback descendingCallback,
					acpi_walk_callback ascendingCallback, void* context,
					void** returnValue);

	status_t	(*get_device)(const char *hid, uint32 index, char *result,
					size_t resultLength);

	status_t	(*get_device_info)(const char *path, char** hid,
					char** cidList, size_t cidListLength, char** uid, char** cls);
	uint32		(*get_object_type)(const char *path);
	status_t	(*get_object)(const char *path,
					acpi_object_type **_returnValue);
	status_t	(*get_object_typed)(const char *path,
					acpi_object_type **_returnValue, uint32 objectType);
	status_t	(*ns_handle_to_pathname)(acpi_handle targetHandle,
					acpi_data *buffer);

	/* Control method execution and data acquisition */

	status_t	(*evaluate_object)(acpi_handle handle, const char* object,
					acpi_objects *args, acpi_object_type *returnValue, size_t bufferLength);
	status_t	(*evaluate_method)(acpi_handle handle, const char *method,
					acpi_objects *args, acpi_data *returnValue);

	/* Resource Management */

	status_t	(*get_irq_routing_table)(acpi_handle busDeviceHandle,
					acpi_data *retBuffer);
	status_t	(*get_current_resources)(acpi_handle busDeviceHandle,
					acpi_data *retBuffer);
	status_t	(*get_possible_resources)(acpi_handle busDeviceHandle,
					acpi_data *retBuffer);
	status_t	(*set_current_resources)(acpi_handle busDeviceHandle,
					acpi_data *buffer);
	status_t	(*walk_resources)(acpi_handle busDeviceHandle,
					char *method, acpi_walk_resources_callback callback,
					void* context);

	/* Power state setting */

	status_t	(*prepare_sleep_state)(uint8 state, void (*wakeFunc)(void),
					size_t size);
	status_t	(*enter_sleep_state)(uint8 state);
	status_t	(*reboot)(void);

	/* Table Access */
	status_t	(*get_table)(const char *signature, uint32 instance,
					void **tableHeader);

	/* Register Access */
	status_t	(*read_bit_register)(uint32 regid, uint32 *val);
	status_t	(*write_bit_register)(uint32 regid, uint32 val);
};


/* Sleep states */

enum {
	ACPI_POWER_STATE_ON = 0,
	ACPI_POWER_STATE_SLEEP_S1,
	ACPI_POWER_STATE_SLEEP_S2,
	ACPI_POWER_STATE_SLEEP_S3,
	ACPI_POWER_STATE_HIBERNATE,
	ACPI_POWER_STATE_OFF
};


#define ACPI_DEVICE_ADDR_ITEM	"acpi/addr"
#define ACPI_DEVICE_CID_ITEM	"acpi/cid"
#define ACPI_DEVICE_HANDLE_ITEM	"acpi/handle"
#define ACPI_DEVICE_HID_ITEM	"acpi/hid"
#define ACPI_DEVICE_PATH_ITEM	"acpi/path"
#define ACPI_DEVICE_TYPE_ITEM	"acpi/type"
#define ACPI_DEVICE_UID_ITEM	"acpi/uid"


typedef struct acpi_device_cookie *acpi_device;

//	Interface to one ACPI device.
typedef struct acpi_device_module_info {
	driver_module_info info;

	/* Notify Handler */

	status_t	(*install_notify_handler)(acpi_device device,
					uint32 handlerType, acpi_notify_handler handler,
					void *context);
	status_t	(*remove_notify_handler)(acpi_device device,
					uint32 handlerType, acpi_notify_handler handler);

	/* Address Space Handler */
	status_t	(*install_address_space_handler)(acpi_device device,
					uint32 spaceId,
					acpi_adr_space_handler handler,
					acpi_adr_space_setup setup,	void *data);
	status_t	(*remove_address_space_handler)(acpi_device device,
					uint32 spaceId,
					acpi_adr_space_handler handler);

	/* Namespace Access */
	uint32		(*get_object_type)(acpi_device device);
	status_t	(*get_object)(acpi_device device, const char *path,
					acpi_object_type **_returnValue);
	status_t	(*walk_namespace)(acpi_device device,
					uint32 objectType, uint32 maxDepth,
					acpi_walk_callback descendingCallback,
					acpi_walk_callback ascendingCallback,
					void* context, void** returnValue);

	/* Control method execution and data acquisition */
	status_t	(*evaluate_method)(acpi_device device, const char *method,
					acpi_objects *args, acpi_data *returnValue);

	/* Resource Management */
	status_t	(*walk_resources)(acpi_device device, char *method,
					acpi_walk_resources_callback callback, void* context);
} acpi_device_module_info;


#endif	/* _ACPI_H */
