// SPDX-License-Identifier: MIT
// haiku's usb module for the freebsd usb shim in usb.cpp, over the usb interfaces
// the xnu side attached on, through the pdusb calls

#include <OS.h>
#include <KernelExport.h>
#include <USB3.h>
#include <stdlib.h>
#include <string.h>

#include "pd_fbsd_usb.h"

// every registered device gets a fresh id, usb.cpp's attach walk starts at the newest one
#define PD_USB_MAX_DEVICES	16
// pipe handles carry the device id above the endpoint, distinct from device ids
#define PD_USB_PIPE_BASE	0x100
#define PD_USB_PIPE_SHIFT	16

struct pd_usb_dev {
	usb_id				id;
	void				*dev;
	struct pdusb_info		info;
	usb_device_descriptor		deviceDesc;
	usb_configuration_descriptor	configDesc;
	usb_interface_descriptor	ifaceDesc;
	usb_endpoint_descriptor		epDesc[PDUSB_MAX_EP];
	usb_endpoint_info		epInfo[PDUSB_MAX_EP];
	usb_interface_info		ifaceInfo;
	usb_interface_list		ifaceList;
	usb_configuration_info		config;
};

static struct pd_usb_dev *sDevs[PD_USB_MAX_DEVICES];
static usb_id sNextId = 1;
static usb_id sNewest;

static struct pd_usb_dev *
find_dev(usb_id id)
{
	for (int i = 0; i < PD_USB_MAX_DEVICES; i++)
		if (sDevs[i] != NULL && sDevs[i]->id == id)
			return sDevs[i];
	return NULL;
}

extern "C" status_t
pd_usb_register(void *dev)
{
	int slot = -1;
	for (int i = 0; i < PD_USB_MAX_DEVICES && slot < 0; i++)
		if (sDevs[i] == NULL)
			slot = i;
	// the walk gives up at 1024
	if (slot < 0 || sNextId >= 1024)
		return B_ERROR;
	struct pd_usb_dev *d = (struct pd_usb_dev *)calloc(1, sizeof(*d));
	if (d == NULL)
		return B_NO_MEMORY;
	if (pdusb_get_info(dev, &d->info) != 0) {
		free(d);
		return B_ERROR;
	}
	d->id = sNextId++;
	d->dev = dev;

	d->deviceDesc.length = sizeof(d->deviceDesc);
	d->deviceDesc.descriptor_type = USB_DESCRIPTOR_DEVICE;
	d->deviceDesc.usb_version = d->info.usb_version;
	d->deviceDesc.device_class = d->info.dev_class;
	d->deviceDesc.device_subclass = d->info.dev_subclass;
	d->deviceDesc.device_protocol = d->info.dev_protocol;
	d->deviceDesc.max_packet_size_0 = d->info.max_packet0;
	d->deviceDesc.vendor_id = d->info.vendor;
	d->deviceDesc.product_id = d->info.product;
	d->deviceDesc.device_version = d->info.release;
	d->deviceDesc.num_configurations = 1;

	d->ifaceDesc.length = sizeof(d->ifaceDesc);
	d->ifaceDesc.descriptor_type = USB_DESCRIPTOR_INTERFACE;
	d->ifaceDesc.interface_number = d->info.if_number;
	d->ifaceDesc.num_endpoints = d->info.nep;
	d->ifaceDesc.interface_class = d->info.if_class;
	d->ifaceDesc.interface_subclass = d->info.if_subclass;
	d->ifaceDesc.interface_protocol = d->info.if_protocol;

	for (int i = 0; i < d->info.nep; i++) {
		d->epDesc[i].length = sizeof(d->epDesc[i]);
		d->epDesc[i].descriptor_type = USB_DESCRIPTOR_ENDPOINT;
		d->epDesc[i].endpoint_address = d->info.ep[i].addr;
		d->epDesc[i].attributes = d->info.ep[i].attrs;
		d->epDesc[i].max_packet_size = d->info.ep[i].max_packet;
		d->epDesc[i].interval = d->info.ep[i].interval;
		d->epInfo[i].descr = &d->epDesc[i];
		d->epInfo[i].handle = (d->id << PD_USB_PIPE_SHIFT) + PD_USB_PIPE_BASE + i;
	}
	d->ifaceInfo.descr = &d->ifaceDesc;
	d->ifaceInfo.handle = d->id;
	d->ifaceInfo.endpoint_count = d->info.nep;
	d->ifaceInfo.endpoint = d->epInfo;
	d->ifaceList.alt_count = 1;
	d->ifaceList.alt = &d->ifaceInfo;
	d->ifaceList.active = &d->ifaceInfo;

	d->configDesc.length = sizeof(d->configDesc);
	d->configDesc.descriptor_type = USB_DESCRIPTOR_CONFIGURATION;
	d->configDesc.number_interfaces = 1;
	d->configDesc.configuration_value = d->info.config_value;
	d->config.descr = &d->configDesc;
	d->config.interface_count = 1;
	d->config.interface = &d->ifaceList;

	sDevs[slot] = d;
	sNewest = d->id;
	return B_OK;
}

// drops the device registered last, its attach failed
extern "C" void
pd_usb_unregister(void)
{
	for (int i = 0; i < PD_USB_MAX_DEVICES; i++) {
		if (sDevs[i] != NULL && sDevs[i]->id == sNewest) {
			free(sDevs[i]);
			sDevs[i] = NULL;
		}
	}
	sNewest = 0;
}

// where usb.cpp's attach walk starts, so it finds only the device being attached
extern "C" uint32
pd_usb_first_device(void)
{
	return sNewest;
}

static struct pd_usb_dev *
pipe_dev(usb_pipe pipe, int *ep)
{
	struct pd_usb_dev *d = find_dev(pipe >> PD_USB_PIPE_SHIFT);
	int i = (int)(pipe & ((1u << PD_USB_PIPE_SHIFT) - 1)) - PD_USB_PIPE_BASE;
	if (d == NULL || i < 0 || i >= d->info.nep)
		return NULL;
	*ep = i;
	return d;
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
	struct pd_usb_dev *d = find_dev(device);
	return d != NULL ? &d->deviceDesc : NULL;
}

static const usb_configuration_info *
get_configuration(usb_device device)
{
	struct pd_usb_dev *d = find_dev(device);
	return d != NULL ? &d->config : NULL;
}

static status_t
send_request(usb_device device, uint8 requestType, uint8 request, uint16 value,
	uint16 index, uint16 length, void *data, size_t *actualLength)
{
	struct pd_usb_dev *d = find_dev(device);
	if (d == NULL)
		return B_DEV_INVALID_PIPE;
	return map_status(pdusb_control(d->dev, requestType, request, value, index,
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
	int ep;
	struct pd_usb_dev *d = pipe_dev(pipe, &ep);
	if (d == NULL)
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

	int status = pdusb_queue(d->dev, ep, buf, x->length, xfer_done, x);
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
	int ep;
	struct pd_usb_dev *d = pipe_dev(pipe, &ep);
	if (d == NULL)
		return B_DEV_INVALID_PIPE;
	return map_status(pdusb_abort(d->dev, ep));
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

extern "C" status_t (*pd_usb_register_hook)(void *dev);
extern "C" void (*pd_usb_unregister_hook)(void);
extern "C" usb_module_info *(*pd_usb_module_hook)(void);

// hands the layer the usb shim, before a usb driver's first attach
extern "C" void
pd_fbsd_usb_install(void)
{
	pd_usb_register_hook = pd_usb_register;
	pd_usb_unregister_hook = pd_usb_unregister;
	pd_usb_module_hook = pd_usb_module;
}
