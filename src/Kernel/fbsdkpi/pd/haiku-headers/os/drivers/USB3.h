/*
 *	Version 3 USB Device Driver API
 *
 *	Copyright 2006, Haiku Inc. All Rights Reserved.
 *	Distributed under the terms of the MIT License.
 */

#ifndef _USB_V3_H_
#define _USB_V3_H_

#include <KernelExport.h>
#include <bus_manager.h>
#include <iovec.h>

#include <USB_spec.h>
#include <USB_isochronous.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_module_info usb_module_info;

typedef uint32 usb_id;
typedef usb_id usb_device;
typedef usb_id usb_interface;
typedef usb_id usb_pipe;

typedef struct usb_endpoint_info usb_endpoint_info;
typedef struct usb_interface_info usb_interface_info;
typedef struct usb_interface_list usb_interface_list;
typedef struct usb_configuration_info usb_configuration_info;

typedef struct usb_notify_hooks {
	status_t		(*device_added)(usb_device device, void **cookie);
	status_t		(*device_removed)(void *cookie);
} usb_notify_hooks;

typedef struct usb_support_descriptor {
	uint8						dev_class;
	uint8						dev_subclass;
	uint8						dev_protocol;
	uint16						vendor;
	uint16						product;
} usb_support_descriptor;

struct usb_endpoint_info {
	usb_endpoint_descriptor		*descr;			/* descriptor and handle */
	usb_pipe					handle;			/* of this endpoint/pipe */
};

struct usb_interface_info {
	usb_interface_descriptor	*descr;			/* descriptor and handle */
	usb_interface				handle;			/* of this interface */

	size_t						endpoint_count;	/* count and list of endpoints */
	usb_endpoint_info			*endpoint;		/* in this interface */

	size_t						generic_count;	/* unparsed descriptors in */
	usb_descriptor				**generic;		/* this interface */
};

struct usb_interface_list {
	size_t						alt_count;		/* count and list of alternate */
	usb_interface_info			*alt;			/* interfaces available */

	usb_interface_info			*active;		/* currently active alternate */
};

struct usb_configuration_info {
	usb_configuration_descriptor *descr;		/* descriptor of this config */

	size_t						interface_count;/* interfaces in this config */
	usb_interface_list			*interface;
};

// Flags for queue_isochronous
#define	USB_ISO_ASAP	0x01

typedef void (*usb_callback_func)(void *cookie, status_t status, void *data,
	size_t actualLength);


/* The usb_module_info represents the public API of the USB Stack. */
struct usb_module_info {
	bus_manager_info				binfo;

	/*
	 *	Use register_driver() to inform the Stack of your interest to support
	 *	USB devices. Support descriptors are used to indicate support for a
	 *	specific device class/subclass/protocol or vendor/product.
	 *	A value of 0 can be used as a wildcard for all fields.
	 *
	 *	Would you like to be notified about all added hubs (class 0x09) you
	 *	would use a support descriptor like this for register_driver:
	 *		usb_support_descriptor hub_devs = { 9, 0, 0, 0, 0 };
	 *
	 *	If you intend to support just any device, or you at least want to be
	 *	notified about any device, just pass NULL as the supportDescriptor and
	 *	0 as the supportDescriptorCount to register_driver.
	 */
	status_t						(*register_driver)(const char *driverName,
										const usb_support_descriptor *supportDescriptors,
										size_t supportDescriptorCount,
										const char *optionalRepublishDriverName);

	/*
	 *	Install notification hooks using your registered driverName. The
	 *	device_added hook will be called for every device that matches the
	 *	support descriptors you provided in register_driver.
	 *	When first installing the hooks you will receive a notification about
	 *	any already present matching device. If you return B_OK in this hook,
	 *	you can use the id that is provided. If the hook indicates an error,
	 *	the resources will be deallocated and you may not use the usb_device
	 *	that is provided. In this case you will not receive a device_removed
	 *	notification for the device either.
	 */
	status_t						(*install_notify)(const char *driverName,
										const usb_notify_hooks *hooks);
	status_t						(*uninstall_notify)(const char *driverName);

	/* Get the device descriptor of a device. */
	const usb_device_descriptor		*(*get_device_descriptor)(usb_device device);

	/* Get the nth supported configuration of a device*/
	const usb_configuration_info	*(*get_nth_configuration)(usb_device device,
										uint32 index);

	/* Get the current configuration */
	const usb_configuration_info	*(*get_configuration)(usb_device device);

	/* Set the current configuration */
	status_t						(*set_configuration)(usb_device device,
										const usb_configuration_info *configuration);

	status_t						(*set_alt_interface)(usb_device device,
										const usb_interface_info *interface);

	/*
	 *	Standard device requests - convenience functions
	 *	The provided handle may be a usb_device, usb_pipe or usb_interface
	 */
	status_t						(*set_feature)(usb_id handle, uint16 selector);
	status_t						(*clear_feature)(usb_id handle, uint16 selector);
	status_t						(*get_status)(usb_id handle, uint16 *status);

	status_t						(*get_descriptor)(usb_device device,
										uint8 descriptorType, uint8 index,
										uint16 languageID, void *data,
										size_t dataLength,
										size_t *actualLength);

	/* Generic device request function - synchronous */
	status_t						(*send_request)(usb_device device,
										uint8 requestType, uint8 request,
										uint16 value, uint16 index,
										uint16 length, void *data,
										size_t *actualLength);

	/*
	 *	Asynchronous request queueing. These functions return immediately
	 *	and the return code only tells whether the _queuing_ of the request
	 *	was successful or not. It doesn't indicate transfer success or error.
	 *
	 *	When the transfer is finished, the provided callback function is
	 *	called with the transfer status, a pointer to the data buffer and
	 *	the actually transfered length as well as with the callbackCookie
	 *	that was used when queuing the request.
	 */
	status_t						(*queue_interrupt)(usb_pipe pipe,
										void *data, size_t dataLength,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_bulk)(usb_pipe pipe,
										void *data, size_t dataLength,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_bulk_v)(usb_pipe pipe,
										iovec *vector, size_t vectorCount,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_bulk_v_physical)(usb_pipe pipe,
										physical_entry *vectors, size_t vectorCount,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_isochronous)(usb_pipe pipe,
										void *data, size_t dataLength,
										usb_iso_packet_descriptor *packetDesc,
										uint32 packetCount,
										uint32 *startingFrameNumber,
										uint32 flags,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_request)(usb_device device,
										uint8 requestType, uint8 request,
										uint16 value, uint16 index,
										uint16 length, void *data,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*set_pipe_policy)(usb_pipe pipe,
										uint8 maxNumQueuedPackets,
										uint16 maxBufferDurationMS,
										uint16 sampleSize);

	/* Cancel all pending async requests in a pipe */
	status_t						(*cancel_queued_transfers)(usb_pipe pipe);

	/* Cancel all pending async requests in a device control pipe */
	status_t						(*cancel_queued_requests)(usb_device device);

	/* Tuning, configuration of timeouts, etc */
	status_t						(*usb_ioctl)(uint32 opcode, void *buffer,
										size_t bufferSize);

	/*
	 * Enumeration of device topology - With these commands you can enumerate
	 * the roothubs and enumerate child devices of hubs. Note that the index
	 * provided to get_nth_child does not map to the port where the child
	 * device is attached to. To get this information you would call
	 * get_device_parent which provides you with the parent hub and the port
	 * index of a device. To test whether an enumerated child is a hub you
	 * can either examine the device descriptor or you can call get_nth_child
	 * and check the returned status. A value of B_OK indicates that the call
	 * succeeded and the returned info is valid, B_ENTRY_NOT_FOUND indicates
	 * that you have reached the last index. Any other error indicates that
	 * the provided arguments are invalid (i.e. not a hub or invalid pointers)
	 * or an internal error occured. Note that there are no guarantees that
	 * the device handle you get stays valid while you are using it. If it gets
	 * invalid any further use will simply return an error. You should install
	 * notify hooks to avoid such situations.
	 */
	status_t						(*get_nth_roothub)(uint32 index,
										usb_device *rootHub);
	status_t						(*get_nth_child)(usb_device hub,
										uint8 index, usb_device *childDevice);
	status_t						(*get_device_parent)(usb_device device,
										usb_device *parentHub,
										uint8 *portIndex);

	/*
	 * Hub interaction - These commands are only valid when used with a hub
	 * device handle. Use reset_port to trigger a reset of the port with index
	 * portIndex. This will cause a disconnect event for the attached device.
	 * With disable_port you can specify that the port at portIndex shall be
	 * disabled. This will also cause a disconnect event for the attached
	 * device. Use reset_port to reenable a previously disabled port.
	 */
	status_t						(*reset_port)(usb_device hub,
										uint8 portIndex);
	status_t						(*disable_port)(usb_device hub,
										uint8 portIndex);
};


#define	B_USB_MODULE_NAME		"bus_managers/usb/v3.1"


#ifdef __cplusplus
}
#endif

#endif /* !_USB_V3_H_ */
