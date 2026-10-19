/*
 * Copyright 2018-2019, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Augustin Cavalier <waddlesplash>
 */
#ifndef _FBSD_COMPAT_USB_STANDARD_H_
#define _FBSD_COMPAT_USB_STANDARD_H_

#include <sys/malloc.h>

MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);

#include <dev/usb/usb_endian.h>
#include <dev/usb/usb_haiku.h>

#define	USB_STACK_VERSION 2000		/* 2.0 */

struct usb_device_request {
	uByte	bmRequestType;
	uByte	bRequest;
	uWord	wValue;
	uWord	wIndex;
	uWord	wLength;
} __packed;
typedef struct usb_device_request usb_device_request_t;

/* abbreviated Haiku request types */
#define HAIKU_USB_REQTYPE_DEVICE_IN				0x80
#define HAIKU_USB_REQTYPE_DEVICE_OUT			0x00
#define HAIKU_USB_REQTYPE_VENDOR				0x40

/* FreeBSD request types */
#define UT_READ_VENDOR_DEVICE	(HAIKU_USB_REQTYPE_DEVICE_IN | HAIKU_USB_REQTYPE_VENDOR)
#define UT_WRITE_VENDOR_DEVICE	(HAIKU_USB_REQTYPE_DEVICE_OUT | HAIKU_USB_REQTYPE_VENDOR)

/* make sure this matches Haiku's usb_speed! */
enum usb_dev_speed {
	USB_SPEED_LOW,
	USB_SPEED_FULL,
	USB_SPEED_HIGH,
	USB_SPEED_SUPER,
};

enum usb_hc_mode {
	USB_MODE_HOST,		/* initiates transfers */
	USB_MODE_DEVICE,	/* bus transfer target */
	USB_MODE_DUAL		/* can be host or device */
};
#define	USB_MODE_MAX	(USB_MODE_DUAL+1)

#define usb_endpoint_descriptor freebsd_usb_endpoint_descriptor
struct usb_endpoint_descriptor {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bEndpointAddress;
#define	UE_GET_DIR(a)	((a) & 0x80)
#define	UE_SET_DIR(a,d)	((a) | (((d)&1) << 7))
#define	UE_DIR_IN	0x80		/* IN-token endpoint, fixed */
#define	UE_DIR_OUT	0x00		/* OUT-token endpoint, fixed */
#define	UE_ADDR		0x0f
#define	UE_ADDR_ANY	0xff
#define	UE_GET_ADDR(a)	((a) & UE_ADDR)
	uByte	bmAttributes;
#define	UE_XFERTYPE	0x03
#define	UE_CONTROL	0x00
#define	UE_BULK	0x02
#define	UE_INTERRUPT	0x03
#define	UE_BULK_INTR	0xfe		/* matches either BULK or INTR */
#define	UE_GET_XFERTYPE(a)	((a) & UE_XFERTYPE)
	uWord	wMaxPacketSize;
	uByte	bInterval;
} __packed;
typedef struct usb_endpoint_descriptor usb_endpoint_descriptor_t;

#endif // _FBSD_COMPAT_USB_STANDARD_H_
