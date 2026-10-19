/*
 * Copyright 2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _USB_RLE_H
#define _USB_RLE_H


#include <SupportDefs.h>


struct _usbd_param_hdr;


/* Run length encoding for isochronous in transfers */

#define RLE_GOOD       1
#define RLE_BAD        2
#define RLE_MISSING    3
#define RLE_UNKNOWN    4

/* data buffer state */
typedef struct rle {
	uint16	rle_status;
	uint16	sample_count;
} rle;

typedef struct rlea {
	uint16	length;
	uint16	num_valid;
	rle		rles[1];
} rlea;


#endif	/* _USB_RLE_H */
