/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _FBSD_COMPAT_USB_DEVICE_H_
#define _FBSD_COMPAT_USB_DEVICE_H_

struct usb_device {
	struct usb_endpoint endpoints[USB_MAX_EP_UNITS];
	uint8_t endpoints_max;		/* number of endpoints present */

	uint32 haiku_usb_device;
};

#endif // _FBSD_COMPAT_USB_DEVICE_H_
