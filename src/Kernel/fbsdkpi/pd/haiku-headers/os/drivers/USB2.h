/*
 *	Version 2 USB Device Driver API
 *
 *	Copyright 2006, Haiku Inc. All Rights Reserved.
 *	Distributed under the terms of the MIT License.
 */

#ifndef _USB_V2_H_
#define _USB_V2_H_

#include <KernelExport.h>
#include <bus_manager.h>

#include <USB_spec.h>
#include <USB_rle.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct usb_module_info usb_module_info;

/* these are opaque handles to internal stack objects */
typedef struct usb_device usb_device;
typedef struct usb_interface usb_interface;
typedef struct usb_pipe usb_pipe;

typedef struct usb_endpoint_info usb_endpoint_info;
typedef struct usb_interface_info usb_interface_info;
typedef struct usb_interface_list usb_interface_list;
typedef struct usb_configuration_info usb_configuration_info;

typedef struct usb_notify_hooks {
	status_t		(*device_added)(const usb_device *device, void **cookie);
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
	usb_pipe					*handle;		/* of this endpoint/pipe */
};

struct usb_interface_info {
	usb_interface_descriptor	*descr;			/* descriptor and handle */
	usb_interface				*handle;		/* of this interface */

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
	 *	would a support descriptor like this to register_driver:
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
	const usb_device_descriptor		*(*get_device_descriptor)(const usb_device *device);

	/* Get the nth supported configuration of a device*/
	const usb_configuration_info	*(*get_nth_configuration)(const usb_device *device,
										uint index);

	/* Get the current configuration */
	const usb_configuration_info	*(*get_configuration)(const usb_device *device);

	/* Set the current configuration */
	status_t						(*set_configuration)(const usb_device *device,
										const usb_configuration_info *configuration);

	status_t						(*set_alt_interface)(const usb_device *device,
										const usb_interface_info *interface);

	/*
	 *	Standard device requests - convenience functions
	 *	The provided object may be a usb_device, usb_pipe or usb_interface
	 */
	status_t						(*set_feature)(const void *object, uint16 selector);
	status_t						(*clear_feature)(const void *object, uint16 selector);
	status_t						(*get_status)(const void *object, uint16 *status);

	status_t						(*get_descriptor)(const usb_device *device,
										uint8 descriptorType, uint8 index,
										uint16 languageID, void *data,
										size_t dataLength,
										size_t *actualLength);

	/* Generic device request function - synchronous */
	status_t						(*send_request)(const usb_device *device,
										uint8 requestType, uint8 request,
										uint16 value, uint16 index,
										uint16 length, void *data,
										size_t dataLength,
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
	status_t						(*queue_interrupt)(const usb_pipe *pipe,
										void *data, size_t dataLength,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_bulk)(const usb_pipe *pipe,
										void *data, size_t dataLength,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_isochronous)(const usb_pipe *pipe,
										void *data, size_t dataLength,
										rlea *rleArray,
										uint16 bufferDurationMS,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*queue_request)(const usb_device *device,
										uint8 requestType, uint8 request,
										uint16 value, uint16 index,
										uint16 length, void *data,
										size_t dataLength,
										usb_callback_func callback,
										void *callbackCookie);

	status_t						(*set_pipe_policy)(const usb_pipe *pipe,
										uint8 maxNumQueuedPackets,
										uint16 maxBufferDurationMS,
										uint16 sampleSize);

	/* Cancel all pending async requests in a pipe */
	status_t						(*cancel_queued_transfers)(const usb_pipe *pipe);

	/* tuning, timeouts, etc */
	status_t						(*usb_ioctl)(uint32 opcode, void *buffer,
										size_t bufferSize);
};


#define	B_USB_MODULE_NAME		"bus_managers/usb/v2"


#ifdef __cplusplus
}
#endif

#endif /* !_USB_V2_H_ */
