// SPDX-License-Identifier: MIT
// haiku's usb module for the freebsd usb shim in usb.cpp, over the one usb interface
// the xnu side attached on, through the pdusb calls

#include <OS.h>
#include <KernelExport.h>
#include <USB3.h>
#include <stdlib.h>
#include <string.h>

#include "pd_fbsd_usb.h"

// usb.cpp walks ids from 0 and takes the first one with a configuration
#define PD_USB_DEVICE		1
// pipe handles, distinct from the device id so a mixed-up handle fails
#define PD_USB_PIPE_BASE	0x100

static void *sDev;
static struct pdusb_info sInfo;
static usb_device_descriptor sDeviceDesc;
static usb_configuration_descriptor sConfigDesc;
static usb_interface_descriptor sIfaceDesc;
static usb_endpoint_descriptor sEpDesc[PDUSB_MAX_EP];
static usb_endpoint_info sEpInfo[PDUSB_MAX_EP];
static usb_interface_info sIfaceInfo;
static usb_interface_list sIfaceList;
static usb_configuration_info sConfig;

extern "C" status_t
pd_usb_register(void *dev)
{
	if (pdusb_get_info(dev, &sInfo) != 0)
		return B_ERROR;
	sDev = dev;

	sDeviceDesc.length = sizeof(sDeviceDesc);
	sDeviceDesc.descriptor_type = USB_DESCRIPTOR_DEVICE;
	sDeviceDesc.usb_version = sInfo.usb_version;
	sDeviceDesc.device_class = sInfo.dev_class;
	sDeviceDesc.device_subclass = sInfo.dev_subclass;
	sDeviceDesc.device_protocol = sInfo.dev_protocol;
	sDeviceDesc.max_packet_size_0 = sInfo.max_packet0;
	sDeviceDesc.vendor_id = sInfo.vendor;
	sDeviceDesc.product_id = sInfo.product;
	sDeviceDesc.device_version = sInfo.release;
	sDeviceDesc.num_configurations = 1;

	sIfaceDesc.length = sizeof(sIfaceDesc);
	sIfaceDesc.descriptor_type = USB_DESCRIPTOR_INTERFACE;
	sIfaceDesc.interface_number = sInfo.if_number;
	sIfaceDesc.num_endpoints = sInfo.nep;
	sIfaceDesc.interface_class = sInfo.if_class;
	sIfaceDesc.interface_subclass = sInfo.if_subclass;
	sIfaceDesc.interface_protocol = sInfo.if_protocol;

	for (int i = 0; i < sInfo.nep; i++) {
		sEpDesc[i].length = sizeof(sEpDesc[i]);
		sEpDesc[i].descriptor_type = USB_DESCRIPTOR_ENDPOINT;
		sEpDesc[i].endpoint_address = sInfo.ep[i].addr;
		sEpDesc[i].attributes = sInfo.ep[i].attrs;
		sEpDesc[i].max_packet_size = sInfo.ep[i].max_packet;
		sEpDesc[i].interval = sInfo.ep[i].interval;
		sEpInfo[i].descr = &sEpDesc[i];
		sEpInfo[i].handle = PD_USB_PIPE_BASE + i;
	}
	sIfaceInfo.descr = &sIfaceDesc;
	sIfaceInfo.handle = PD_USB_DEVICE;
	sIfaceInfo.endpoint_count = sInfo.nep;
	sIfaceInfo.endpoint = sEpInfo;
	sIfaceList.alt_count = 1;
	sIfaceList.alt = &sIfaceInfo;
	sIfaceList.active = &sIfaceInfo;

	sConfigDesc.length = sizeof(sConfigDesc);
	sConfigDesc.descriptor_type = USB_DESCRIPTOR_CONFIGURATION;
	sConfigDesc.number_interfaces = 1;
	sConfigDesc.configuration_value = sInfo.config_value;
	sConfig.descr = &sConfigDesc;
	sConfig.interface_count = 1;
	sConfig.interface = &sIfaceList;
	return B_OK;
}

extern "C" void
pd_usb_unregister(void)
{
	sDev = NULL;
}

static int
pipe_index(usb_pipe pipe)
{
	int i = (int)pipe - PD_USB_PIPE_BASE;
	return i >= 0 && i < sInfo.nep ? i : -1;
}

static status_t
map_status(int status)
{
	switch (status) {
	case PDUSB_OK:
		return B_OK;
	case PDUSB_STALLED:
		return B_DEV_STALLED;
	case PDUSB_CANCELLED:
		return B_CANCELED;
	case PDUSB_TIMEOUT:
		return B_TIMED_OUT;
	default:
		return B_ERROR;
	}
}

static const usb_device_descriptor *
get_device_descriptor(usb_device device)
{
	return sDev != NULL && device == PD_USB_DEVICE ? &sDeviceDesc : NULL;
}

static const usb_configuration_info *
get_configuration(usb_device device)
{
	return sDev != NULL && device == PD_USB_DEVICE ? &sConfig : NULL;
}

static status_t
send_request(usb_device device, uint8 requestType, uint8 request, uint16 value,
	uint16 index, uint16 length, void *data, size_t *actualLength)
{
	if (sDev == NULL || device != PD_USB_DEVICE)
		return B_DEV_INVALID_PIPE;
	return map_status(pdusb_control(sDev, requestType, request, value, index,
		length, data, actualLength));
}

// a bounce buffer for transfers spread over several iovecs
struct pd_usb_xfer {
	usb_callback_func	callback;
	void			*cookie;
	iovec			*vec;
	size_t			count;
	// the single buffer of an interrupt transfer
	iovec			one;
	void			*bounce;
	size_t			length;
};

static void
xfer_done(void *arg, int status, size_t actual)
{
	struct pd_usb_xfer *x = (struct pd_usb_xfer *)arg;

	if (x->bounce != NULL) {
		size_t left = actual;
		uint8 *from = (uint8 *)x->bounce;
		for (size_t i = 0; i < x->count && left > 0; i++) {
			size_t n = x->vec[i].iov_len < left ? x->vec[i].iov_len : left;
			memcpy(x->vec[i].iov_base, from, n);
			from += n;
			left -= n;
		}
		free(x->bounce);
	}
	usb_callback_func callback = x->callback;
	void *cookie = x->cookie;
	void *data = x->count == 1 ? x->vec[0].iov_base : NULL;
	free(x);
	callback(cookie, map_status(status), data, actual);
}

static status_t
queue(usb_pipe pipe, iovec *vec, size_t count, void *data, size_t dataLength,
	usb_callback_func callback, void *cookie)
{
	int ep = pipe_index(pipe);
	if (sDev == NULL || ep < 0)
		return B_DEV_INVALID_PIPE;

	struct pd_usb_xfer *x = (struct pd_usb_xfer *)calloc(1, sizeof(*x));
	if (x == NULL)
		return B_NO_MEMORY;
	x->callback = callback;
	x->cookie = cookie;
	if (vec == NULL) {
		x->one.iov_base = data;
		x->one.iov_len = dataLength;
		vec = &x->one;
		count = 1;
	}
	x->vec = vec;
	x->count = count;
	for (size_t i = 0; i < count; i++)
		x->length += vec[i].iov_len;

	void *buf = count == 1 ? vec[0].iov_base : NULL;
	if (count != 1) {
		x->bounce = malloc(x->length > 0 ? x->length : 1);
		if (x->bounce == NULL) {
			free(x);
			return B_NO_MEMORY;
		}
		uint8 *to = (uint8 *)x->bounce;
		for (size_t i = 0; i < count; i++) {
			memcpy(to, vec[i].iov_base, vec[i].iov_len);
			to += vec[i].iov_len;
		}
		buf = x->bounce;
	}

	int status = pdusb_queue(sDev, ep, buf, x->length, xfer_done, x);
	if (status != PDUSB_OK) {
		free(x->bounce);
		free(x);
		return map_status(status);
	}
	return B_OK;
}

static status_t
queue_bulk_v(usb_pipe pipe, iovec *vector, size_t vectorCount,
	usb_callback_func callback, void *callbackCookie)
{
	return queue(pipe, vector, vectorCount, NULL, 0, callback, callbackCookie);
}

static status_t
queue_interrupt(usb_pipe pipe, void *data, size_t dataLength,
	usb_callback_func callback, void *callbackCookie)
{
	return queue(pipe, NULL, 0, data, dataLength, callback, callbackCookie);
}

static status_t
cancel_queued_transfers(usb_pipe pipe)
{
	int ep = pipe_index(pipe);
	if (sDev == NULL || ep < 0)
		return B_DEV_INVALID_PIPE;
	return map_status(pdusb_abort(sDev, ep));
}

static usb_module_info sUSBModule;

extern "C" usb_module_info *
pd_usb_module(void)
{
	sUSBModule.binfo.minfo.name = B_USB_MODULE_NAME;
	sUSBModule.get_device_descriptor = get_device_descriptor;
	sUSBModule.get_configuration = get_configuration;
	sUSBModule.send_request = send_request;
	sUSBModule.queue_bulk_v = queue_bulk_v;
	sUSBModule.queue_interrupt = queue_interrupt;
	sUSBModule.cancel_queued_transfers = cancel_queued_transfers;
	return &sUSBModule;
}
