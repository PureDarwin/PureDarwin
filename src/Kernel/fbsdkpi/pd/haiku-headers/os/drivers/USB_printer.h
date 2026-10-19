/*
 * Copyright 2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _USB_PRINTER_H
#define _USB_PRINTER_H


#include <Drivers.h>


enum {
	USB_PRINTER_GET_DEVICE_ID = B_DEVICE_OP_CODES_END + 1
};

#define USB_PRINTER_DEVICE_ID_LENGTH	256
	/* buffer size to be handed to above ioctl() call */


#endif	/* _USB_PRINTER_H */
