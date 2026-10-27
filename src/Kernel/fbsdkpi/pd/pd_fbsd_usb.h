// SPDX-License-Identifier: MIT
// the usb device a FreeBSD usb driver runs on, carried out by the xnu side over IOUSBFamily
// fixed-width types only, both sides include it
#ifndef PD_FBSD_USB_H
#define PD_FBSD_USB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PDUSB_MAX_EP 16

struct pdusb_ep {
	uint8_t		addr;
	uint8_t		attrs;
	uint16_t	max_packet;
	uint8_t		interval;
};

// the device and the one interface the driver was matched on
struct pdusb_info {
	uint16_t	vendor;
	uint16_t	product;
	uint16_t	release;
	uint16_t	usb_version;
	uint8_t		dev_class;
	uint8_t		dev_subclass;
	uint8_t		dev_protocol;
	uint8_t		max_packet0;
	uint8_t		config_value;
	uint8_t		if_number;
	uint8_t		if_class;
	uint8_t		if_subclass;
	uint8_t		if_protocol;
	int		nep;
	struct pdusb_ep	ep[PDUSB_MAX_EP];
};

enum {
	PDUSB_OK = 0,
	PDUSB_STALLED,
	PDUSB_CANCELLED,
	PDUSB_TIMEOUT,
	PDUSB_ERROR,
};

typedef void (*pdusb_done_t)(void *cookie, int status, size_t actual);

// xnu side: wraps an opened IOUSBInterface, close aborts every endpoint first
void *pdusb_open(void *iface);
void pdusb_close(void *dev);

// dev is the opaque handle the xnu side passed to pd_fbsd_attach_usb
int pdusb_get_info(void *dev, struct pdusb_info *info);
// a synchronous control request on pipe zero
int pdusb_control(void *dev, uint8_t type, uint8_t request, uint16_t value,
	uint16_t index, uint16_t length, void *data, size_t *actual);
// a bulk or interrupt transfer on endpoint ep (an index into info.ep), done runs on the usb thread
int pdusb_queue(void *dev, int ep, void *buf, size_t len, pdusb_done_t done, void *cookie);
// cancels the endpoint's transfers and returns once every one of them has completed
int pdusb_abort(void *dev, int ep);

#ifdef __cplusplus
}
#endif

#endif
