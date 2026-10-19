/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */

#include <sys/cdefs.h>
#include <sys/callout.h>
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>


/** device_set_usb_desc
 * This function can be called at probe or attach to set the USB
 * device supplied textual description for the given device. */
void
device_set_usb_desc(device_t dev)
{
	struct usb_attach_arg *uaa;
	struct usb_device *udev;
	struct usb_interface *iface;
	usb_error_t err;

	if (dev == NULL) {
		/* should not happen */
		return;
	}
	uaa = device_get_ivars(dev);
	if (uaa == NULL) {
		/* can happen if called at the wrong time */
		return;
	}
	udev = uaa->device;
	iface = uaa->iface;

	if ((iface == NULL)) {
		err = USB_ERR_INVAL;
	} else {
		err = 0;
	}

	/* TODO */
}
