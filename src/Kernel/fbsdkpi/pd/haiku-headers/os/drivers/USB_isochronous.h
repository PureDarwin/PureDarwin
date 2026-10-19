/*
 * Copyright 2021, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _USB_ISOCHRONOUS_H
#define _USB_ISOCHRONOUS_H


#include <SupportDefs.h>


typedef struct {
	int16						request_length;
	int16						actual_length;
	status_t					status;
} usb_iso_packet_descriptor;


#endif	/* _USB_ISOCHRONOUS_H */
