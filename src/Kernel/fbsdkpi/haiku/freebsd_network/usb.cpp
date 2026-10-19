/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */

#include <sys/condvar.h>

extern "C" {
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/priority.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_device.h>

#include "device.h"
}

// undo name remappings, so we can use both FreeBSD and Haiku ones in this file
#undef usb_device
#undef usb_interface
#undef usb_endpoint_descriptor

#include <USB3.h>


struct mtx sUSBLock;
usb_module_info* sUSB = NULL;
struct taskqueue* sUSBTaskqueue = NULL;


status_t
init_usb()
{
	if (sUSB != NULL)
		return B_OK;

	if (get_module(B_USB_MODULE_NAME, (module_info**)&sUSB) != B_OK) {
		dprintf("cannot get module \"%s\"\n", B_USB_MODULE_NAME);
		return B_ERROR;
	}

	mtx_init(&sUSBLock, "fbsd usb", NULL, MTX_DEF);
	return B_OK;
}


void
uninit_usb()
{
	if (sUSB == NULL)
		return;

	put_module(B_USB_MODULE_NAME);
	if (sUSBTaskqueue != NULL)
		taskqueue_free(sUSBTaskqueue);

	sUSB = NULL;
	sUSBTaskqueue = NULL;
	mtx_destroy(&sUSBLock);
}


static status_t
get_next_usb_device(uint32* cookie, freebsd_usb_device* result)
{
	// We cheat here: since USB IDs are sequential, instead of doing a
	// complicated parent/child iteration dance, we simply request device
	// descriptors and let the USB stack figure out the rest.
	//
	// It would be better if we used USB->register_driver, but that is not
	// an option at present for a variety of reasons...
	const usb_configuration_info* config;
	usb_device current;
	while (*cookie < 1024) {
		current = *cookie;
		*cookie = *cookie + 1;

		config = sUSB->get_configuration(current);
		if (config != NULL)
			break;
	}
	if (config == NULL)
		return ENODEV;

	result->haiku_usb_device = current;
	result->endpoints_max = 0;
	for (size_t i = 0; i < config->interface_count; i++) {
		usb_interface_info* iface = config->interface[i].active;
		if (iface == NULL)
			continue;

		for (size_t j = 0; j < iface->endpoint_count; j++) {
			if (iface->endpoint[j].descr == NULL)
				continue;

			const int rep = result->endpoints_max++;
			result->endpoints[rep].iface_index = i;

			static_assert(sizeof(freebsd_usb_endpoint_descriptor)
				== sizeof(usb_endpoint_descriptor), "size mismatch");

			if (result->endpoints[rep].edesc == NULL)
				result->endpoints[rep].edesc = new freebsd_usb_endpoint_descriptor;

			memcpy(result->endpoints[rep].edesc, iface->endpoint[j].descr,
				sizeof(usb_endpoint_descriptor));
		}
	}

	return B_OK;
}


static status_t
get_usb_device_attach_arg(struct freebsd_usb_device* device, struct usb_attach_arg* uaa)
{
	memset(uaa, 0, sizeof(struct usb_attach_arg));

	const usb_device_descriptor* device_desc =
		sUSB->get_device_descriptor(device->haiku_usb_device);
	if (device_desc == NULL)
		return B_BAD_VALUE;

	uaa->info.idVendor = device_desc->vendor_id;
	uaa->info.idProduct = device_desc->product_id;
	uaa->info.bcdDevice = device_desc->device_version;
	uaa->info.bDeviceClass = device_desc->device_class;
	uaa->info.bDeviceSubClass = device_desc->device_subclass;
	uaa->info.bDeviceProtocol = device_desc->device_protocol;

	const usb_configuration_info* config = sUSB->get_configuration(device->haiku_usb_device);
	if (device_desc == NULL)
		return B_BAD_VALUE;

	// TODO: represent more than just interface[0], but how?
	usb_interface_info* iface = config->interface[0].active;
	if (iface == NULL)
		return B_NO_INIT;

	uaa->info.bInterfaceClass = iface->descr->interface_class;
	uaa->info.bInterfaceSubClass = iface->descr->interface_subclass;
	uaa->info.bInterfaceProtocol = iface->descr->interface_protocol;

	// TODO: bIface{Index,Num}, bConfig{Index,Num}

	uaa->device = device;
	uaa->iface = NULL;

	// TODO: fetch values for these?
	uaa->usb_mode = USB_MODE_HOST;
	uaa->port = 1;
	uaa->dev_state = UAA_DEV_READY;

	return B_OK;
}


static void
usb_cleanup_device(freebsd_usb_device* udev)
{
	for (int i = 0; i < USB_MAX_EP_UNITS; i++) {
		delete udev->endpoints[i].edesc;
		udev->endpoints[i].edesc = NULL;
	}
}


struct compat_usb_device {
	freebsd_usb_device udev;
	struct usb_attach_arg uaa;
};


static void
free_compat_usb_device(void* cookie)
{
	compat_usb_device* compat_device = (compat_usb_device*)cookie;
	usb_cleanup_device(&compat_device->udev);
	free(compat_device);
}


static void
prepare_usb_attach(void* cookie, device_t device)
{
	compat_usb_device* compat_device = (compat_usb_device*)cookie;

	struct root_device_softc* root_softc
		= (struct root_device_softc*)device->parent->softc;
	root_softc->usb_dev = &compat_device->udev;
	device_set_ivars(device, &compat_device->uaa);
}


status_t
_fbsd_init_hardware_uhub(driver_t* drivers[])
{
	status_t status;
	device_t root;
	const int BUS_uhub = root_device_softc::BUS_uhub;

	status = init_usb();
	if (status != B_OK)
		return status;

	status = init_root_device(&root, BUS_uhub);
	if (status != B_OK)
		return status;

	bool found = false;
	uint32 cookie = 0;
	struct freebsd_usb_device udev = {};
	while ((status = get_next_usb_device(&cookie, &udev)) == B_OK) {
		int best = 0;
		driver_t* driver = NULL;

		struct usb_attach_arg uaa;
		status = get_usb_device_attach_arg(&udev, &uaa);
		if (status != B_OK)
			continue;

		struct device device = {};
		device.parent = root;
		device.root = root;
		device_set_ivars(&device, &uaa);

		driver = __haiku_probe_drivers(&device, drivers);
		if (driver == NULL)
			continue;

		compat_usb_device* compat_device = (compat_usb_device*)malloc(sizeof(compat_usb_device));
		compat_device->udev = udev;
		compat_device->uaa = uaa;
		compat_device->uaa.device = &compat_device->udev;

		// We just "transferred ownership" of usb_dev to sProbedDevices.
		memset(&udev, 0, sizeof(udev));

		report_probed_device(BUS_uhub, compat_device, driver,
			prepare_usb_attach, free_compat_usb_device);
		found = true;
	}

	device_delete_child(NULL, root);
	usb_cleanup_device(&udev);

	if (found)
		return B_OK;

	uninit_usb();
	return B_NOT_SUPPORTED;
}


static usb_error_t
map_usb_error(status_t err)
{
	switch (err) {
	case B_OK:			return USB_ERR_NORMAL_COMPLETION;
	case B_DEV_STALLED:	return USB_ERR_STALLED;
	case B_CANCELED:	return USB_ERR_CANCELLED;
	case B_TIMED_OUT:	return USB_ERR_TIMEOUT;
	}
	return USB_ERR_INVAL;
}


extern "C" usb_error_t
usbd_do_request_flags(struct freebsd_usb_device* udev, struct mtx* mtx,
	struct usb_device_request* req, void* data, uint16_t flags,
	uint16_t* actlen, usb_timeout_t timeout)
{
	if (mtx != NULL)
		mtx_unlock(mtx);

	// FIXME: timeouts
	// TODO: flags

	size_t actualLen = 0;
	status_t ret = sUSB->send_request((usb_device)udev->haiku_usb_device,
		req->bmRequestType, req->bRequest,
		UGETW(req->wValue), UGETW(req->wIndex), UGETW(req->wLength),
		data, &actualLen);
	if (actlen)
		*actlen = actualLen;

	if (mtx != NULL)
		mtx_lock(mtx);

	return map_usb_error(ret);
}


enum usb_dev_speed
usbd_get_speed(struct freebsd_usb_device* udev)
{
	const usb_device_descriptor* descriptor = sUSB->get_device_descriptor(
		(usb_device)udev->haiku_usb_device);
	KASSERT(descriptor != NULL, ("no device"));

	if (descriptor->usb_version >= 0x0300)
		return USB_SPEED_SUPER;
	else if (descriptor->usb_version >= 0x200)
		return USB_SPEED_HIGH;
	else if (descriptor->usb_version >= 0x110)
		return USB_SPEED_FULL;
	else if (descriptor->usb_version >= 0x100)
		return USB_SPEED_LOW;

	panic("unknown USB version!");
	return (usb_dev_speed)-1;
}


struct usb_page_cache {
	void* buffer;
	size_t length;
};

struct usb_xfer {
	struct mtx* mutex;
	void* priv_sc, *priv;
	usb_callback_t* callback;
	usb_xfer_flags flags;
	usb_frlength_t max_data_length;

	usb_device device;
	uint8 type;
	usb_pipe pipe;

	iovec* frames;
	usb_page_cache* buffers;
	int max_frame_count, nframes;

	uint8 usb_state;
	bool in_progress;
	status_t result;
	int transferred_length;

	struct task invoker;
	struct cv condition;
};


extern "C" usb_error_t
usbd_transfer_setup(struct freebsd_usb_device* udev,
	const uint8_t* ifaces, struct usb_xfer** ppxfer,
	const struct usb_config* setup_start, uint16_t n_setup,
	void* priv_sc, struct mtx* xfer_mtx)
{
	if (xfer_mtx == NULL)
		xfer_mtx = &Giant;

	// Make sure the taskqueue exists.
	if (sUSBTaskqueue == NULL) {
		mtx_lock(&sUSBLock);
		if (sUSBTaskqueue == NULL) {
			sUSBTaskqueue = taskqueue_create("usb taskq", 0,
				taskqueue_thread_enqueue, &sUSBTaskqueue);
			taskqueue_start_threads(&sUSBTaskqueue, 1, PZERO, "usb taskq");
		}
		mtx_unlock(&sUSBLock);
	}

	const usb_configuration_info* device_config = sUSB->get_configuration(
		(usb_device)udev->haiku_usb_device);

	for (const struct usb_config* setup = setup_start;
			setup < (setup_start + n_setup); setup++) {
		/* skip transfers w/o callbacks */
		if (setup->callback == NULL)
			continue;

		struct usb_xfer* xfer = new usb_xfer;
		xfer->mutex = xfer_mtx;
		xfer->priv_sc = priv_sc;
		xfer->priv = NULL;
		xfer->callback = setup->callback;
		xfer->flags = setup->flags;
		xfer->max_data_length = setup->bufsize;

		xfer->device = (usb_device)udev->haiku_usb_device;
		xfer->type = setup->type;

		xfer->pipe = -1;
		uint8_t endpoint = setup->endpoint;
		uint8_t iface_index = ifaces[setup->if_index];
		if (endpoint == UE_ADDR_ANY) {
			for (int i = 0; i < udev->endpoints_max; i++) {
				const uint8 type = UE_GET_XFERTYPE(udev->endpoints[i].edesc->bmAttributes);
				const uint8 dir = UE_GET_DIR(udev->endpoints[i].edesc->bEndpointAddress);
				if (type != xfer->type
						&& !(xfer->type == UE_BULK_INTR
							&& (type == UE_BULK || type == UE_INTERRUPT)))
					continue;
				if (dir != setup->direction)
					continue;

				endpoint = udev->endpoints[i].edesc->bEndpointAddress;
				xfer->type = type;
				break;
			}
		}
		usb_interface_info* iface = device_config->interface[iface_index].active;
		for (int i = 0; i < iface->endpoint_count; i++) {
			if (iface->endpoint[i].descr->endpoint_address != endpoint)
				continue;

			xfer->pipe = iface->endpoint[i].handle;
			if (xfer->max_data_length == 0)
				xfer->max_data_length = iface->endpoint[i].descr->max_packet_size;
			break;
		}
		if (xfer->pipe == -1)
			panic("failed to locate endpoint!");

		xfer->nframes = setup->frames;
		if (xfer->nframes == 0)
			xfer->nframes = 1;
		xfer->max_frame_count = xfer->nframes;
		xfer->frames = (iovec*)calloc(xfer->max_frame_count, sizeof(iovec));
		xfer->buffers = NULL;

		xfer->usb_state = USB_ST_SETUP;
		xfer->in_progress = false;
		xfer->transferred_length = 0;
		cv_init(&xfer->condition, "FreeBSD USB transfer");

		if (xfer->flags.proxy_buffer)
			panic("not yet supported");

		ppxfer[setup - setup_start] = xfer;
	}

	return USB_ERR_NORMAL_COMPLETION;
}


extern "C" void
usbd_transfer_unsetup(struct usb_xfer** pxfer, uint16_t n_setup)
{
	for (int i = 0; i < n_setup; i++) {
		struct usb_xfer* xfer = pxfer[i];
		usbd_transfer_drain(xfer);
		cv_destroy(&xfer->condition);

		if (xfer->buffers != NULL) {
			for (int i = 0; i < xfer->max_frame_count; i++)
				free(xfer->buffers[i].buffer);
			free(xfer->buffers);
		}
		free(xfer->frames);
		delete xfer;
	}
}


extern "C" usb_frlength_t
usbd_xfer_max_len(struct usb_xfer* xfer)
{
	return xfer->max_data_length;
}


extern "C" void*
usbd_xfer_softc(struct usb_xfer* xfer)
{
	return xfer->priv_sc;
}


extern "C" void*
usbd_xfer_get_priv(struct usb_xfer* xfer)
{
	return xfer->priv;
}


extern "C" void
usbd_xfer_set_priv(struct usb_xfer* xfer, void* ptr)
{
	xfer->priv = ptr;
}


extern "C" uint8_t
usbd_xfer_state(struct usb_xfer* xfer)
{
	return xfer->usb_state;
}


extern "C" void
usbd_xfer_set_frames(struct usb_xfer* xfer, usb_frcount_t n)
{
	KASSERT(n <= uint32_t(xfer->max_frame_count), ("frame index overflow"));
	xfer->nframes = n;
}


extern "C" void
usbd_xfer_set_frame_data(struct usb_xfer* xfer,
	usb_frcount_t frindex, void* ptr, usb_frlength_t len)
{
	KASSERT(frindex < uint32_t(xfer->nframes), ("frame index overflow"));

	xfer->frames[frindex].iov_base = ptr;
	xfer->frames[frindex].iov_len = len;
}


extern "C" void
usbd_xfer_set_frame_len(struct usb_xfer* xfer,
	usb_frcount_t frindex, usb_frlength_t len)
{
	KASSERT(frindex < uint32_t(xfer->max_frame_count), ("frame index overflow"));
	KASSERT(len <= uint32_t(xfer->max_data_length), ("length overflow"));

	// Trigger buffer allocation if necessary.
	if (xfer->frames[frindex].iov_base == NULL)
		usbd_xfer_get_frame(xfer, frindex);

	xfer->frames[frindex].iov_len = len;
}


extern "C" struct usb_page_cache*
usbd_xfer_get_frame(struct usb_xfer* xfer, usb_frcount_t frindex)
{
	KASSERT(frindex < uint32_t(xfer->max_frame_count), ("frame index overflow"));
	if (xfer->buffers == NULL)
		xfer->buffers = (usb_page_cache*)calloc(xfer->max_frame_count, sizeof(usb_page_cache));

	usb_page_cache* cache = &xfer->buffers[frindex];
	if (cache->buffer == NULL) {
		cache->buffer = malloc(xfer->max_data_length);
		cache->length = xfer->max_data_length;
	}

	xfer->frames[frindex].iov_base = cache->buffer;
	return cache;
}


extern "C" void *
usbd_xfer_get_frame_buffer(struct usb_xfer *xfer, usb_frcount_t frindex)
{
	return usbd_xfer_get_frame(xfer, frindex)->buffer;
}


extern "C" void
usbd_frame_zero(struct usb_page_cache* cache,
	usb_frlength_t offset, usb_frlength_t len)
{
	KASSERT((offset + len) < uint32_t(cache->length), ("buffer overflow"));
	memset((uint8*)cache->buffer + offset, 0, len);
}


extern "C" void
usbd_copy_in(struct usb_page_cache* cache, usb_frlength_t offset,
	const void *ptr, usb_frlength_t len)
{
	KASSERT((offset + len) < uint32_t(cache->length), ("buffer overflow"));
	memcpy((uint8*)cache->buffer + offset, ptr, len);
}


extern "C" void
usbd_copy_out(struct usb_page_cache* cache, usb_frlength_t offset,
	void *ptr, usb_frlength_t len)
{
	KASSERT((offset + len) < uint32_t(cache->length), ("buffer overflow"));
	memcpy(ptr, (uint8*)cache->buffer + offset, len);
}


extern "C" void
usbd_m_copy_in(struct usb_page_cache* cache, usb_frlength_t dst_offset,
	struct mbuf *m, usb_size_t src_offset, usb_frlength_t src_len)
{
	m_copydata(m, src_offset, src_len, (caddr_t)((uint8*)cache->buffer + dst_offset));
}


extern "C" void
usbd_xfer_set_stall(struct usb_xfer *xfer)
{
	// Not needed?
}


static void
usbd_invoker(void* arg, int pending)
{
	struct usb_xfer* xfer = (struct usb_xfer*)arg;
	mtx_lock(xfer->mutex);
	xfer->in_progress = false;
	xfer->usb_state = (xfer->result == B_OK) ? USB_ST_TRANSFERRED : USB_ST_ERROR;
	xfer->callback(xfer, map_usb_error(xfer->result));
	mtx_unlock(xfer->mutex);
	cv_signal(&xfer->condition);
}


static void
usbd_callback(void* arg, status_t status, void* data, size_t actualLength)
{
	struct usb_xfer* xfer = (struct usb_xfer*)arg;
	xfer->result = status;
	xfer->transferred_length = actualLength;

	TASK_INIT(&xfer->invoker, 0, usbd_invoker, xfer);
	taskqueue_enqueue(sUSBTaskqueue, &xfer->invoker);
}


extern "C" void
usbd_transfer_start(struct usb_xfer* xfer)
{
	if (xfer->in_progress)
		return;

	xfer->usb_state = USB_ST_SETUP;
	xfer->callback(xfer, USB_ERR_NOT_STARTED);
}


extern "C" void
usbd_transfer_submit(struct usb_xfer* xfer)
{
	KASSERT(!xfer->in_progress, ("cannot submit in-progress transfer!"));

	xfer->transferred_length = 0;
	xfer->in_progress = true;
	status_t status = B_NOT_SUPPORTED;
	switch (xfer->type) {
	case UE_BULK:
		status = sUSB->queue_bulk_v(xfer->pipe, xfer->frames, xfer->nframes, usbd_callback, xfer);
		break;

	case UE_INTERRUPT:
		KASSERT(xfer->nframes == 1, ("invalid frame count for interrupt transfer"));
		status = sUSB->queue_interrupt(xfer->pipe,
			xfer->frames[0].iov_base, xfer->frames[0].iov_len,
			usbd_callback, xfer);
		break;

	default:
		panic("unhandled pipe type %d", xfer->type);
	}

	if (status != B_OK)
		usbd_callback(xfer, status, NULL, 0);
}


extern "C" void
usbd_transfer_stop(struct usb_xfer* xfer)
{
	if (xfer == NULL)
		return;
	mtx_assert(xfer->mutex, MA_OWNED);

	if (!xfer->in_progress)
		return;

	// Unfortunately we have no way of cancelling just one transfer.
	sUSB->cancel_queued_transfers(xfer->pipe);
}


extern "C" void
usbd_transfer_drain(struct usb_xfer* xfer)
{
	if (xfer == NULL)
		return;

	mtx_lock(xfer->mutex);
	usbd_transfer_stop(xfer);
	while (xfer->in_progress)
		cv_wait(&xfer->condition, xfer->mutex);
	mtx_unlock(xfer->mutex);
}


extern "C" void
usbd_xfer_status(struct usb_xfer* xfer, int* actlen, int* sumlen, int* aframes, int* nframes)
{
	if (actlen)
		*actlen = xfer->transferred_length;
	if (sumlen) {
		int sum = 0;
		for (int i = 0; i < xfer->nframes; i++)
			sum += xfer->frames[i].iov_len;
		*sumlen = sum;
	}
	if (aframes) {
		int length = xfer->transferred_length;
		int frames = 0;
		for (int i = 0; i < xfer->nframes && length > 0; i++) {
			length -= xfer->frames[i].iov_len;
			if (length >= 0)
				frames++;
		}
		*aframes = frames;
	}
	if (nframes)
		*nframes = xfer->nframes;
}
