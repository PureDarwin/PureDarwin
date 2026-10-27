// SPDX-License-Identifier: MIT
// the usb side of fbsdkpi: one IOUSBInterface, its pipes and its device's control pipe,
// as the pdusb calls pd_usb.cpp turns into haiku's usb module

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <libkern/OSAtomic.h>

#include "pd_fbsd_usb.h"

struct pdusb_pipe {
	IOUSBPipe		*pipe;
	bool			in;
	volatile SInt32		inflight;
};

struct pdusb_dev {
	IOUSBInterface		*iface;
	IOUSBDevice		*device;
	// one control request at a time, the xhci pipe zero path does not take concurrent ones
	IOLock			*control_lock;
	volatile bool		dying;
	struct pdusb_info	info;
	struct pdusb_pipe	pipes[PDUSB_MAX_EP];
};

struct pdusb_xfer {
	struct pdusb_dev	*dev;
	struct pdusb_pipe	*pipe;
	IOBufferMemoryDescriptor *mem;
	void			*buf;
	size_t			len;
	pdusb_done_t		done;
	void			*cookie;
	IOUSBCompletion		completion;
};

static uint64_t
number_property(IOService *service, const char *key)
{
	OSNumber *n = OSDynamicCast(OSNumber, service->getProperty(key));
	return n != NULL ? n->unsigned64BitValue() : 0;
}

extern "C" void *
pdusb_open(void *ifacep)
{
	IOUSBInterface *iface = (IOUSBInterface *)ifacep;
	struct pdusb_dev *dev = (struct pdusb_dev *)IOMallocZero(sizeof(*dev));

	if (dev == NULL)
		return NULL;
	dev->iface = iface;
	dev->device = iface->GetDevice();
	dev->control_lock = IOLockAlloc();
	if (dev->control_lock == NULL) {
		IOFree(dev, sizeof(*dev));
		return NULL;
	}

	// the descriptor fields the device publishes, IOUSBFamily's inline getters are not exported to an aux KC
	struct pdusb_info *info = &dev->info;
	info->vendor = (uint16_t)number_property(dev->device, kUSBVendorID);
	info->product = (uint16_t)number_property(dev->device, kUSBProductID);
	info->release = (uint16_t)number_property(dev->device, kUSBDeviceReleaseNumber);
	info->dev_class = (uint8_t)number_property(dev->device, kUSBDeviceClass);
	info->dev_subclass = (uint8_t)number_property(dev->device, kUSBDeviceSubClass);
	info->dev_protocol = (uint8_t)number_property(dev->device, kUSBDeviceProtocol);
	info->max_packet0 = dev->device->GetMaxPacketSize();
	// bcdUSB is not published, the usb shim only reads it for the link speed
	switch (dev->device->GetSpeed()) {
	case kUSBDeviceSpeedSuper:
		info->usb_version = 0x0300;
		break;
	case kUSBDeviceSpeedHigh:
		info->usb_version = 0x0200;
		break;
	case kUSBDeviceSpeedFull:
		info->usb_version = 0x0110;
		break;
	default:
		info->usb_version = 0x0100;
		break;
	}
	info->config_value = iface->GetConfigValue();
	info->if_number = iface->GetInterfaceNumber();
	info->if_class = iface->GetInterfaceClass();
	info->if_subclass = iface->GetInterfaceSubClass();
	info->if_protocol = iface->GetInterfaceProtocol();

	// the pipe list is not packed from index 0, so walk all of it
	for (UInt8 i = 0; i < kUSBMaxPipes && info->nep < PDUSB_MAX_EP; i++) {
		IOUSBPipe *pipe = iface->GetPipeObj(i);
		const IOUSBEndpointDescriptor *ed = pipe ? pipe->GetEndpointDescriptor() : NULL;

		if (ed == NULL)
			continue;
		struct pdusb_ep *ep = &info->ep[info->nep];
		ep->addr = ed->bEndpointAddress;
		ep->attrs = ed->bmAttributes;
		ep->max_packet = OSSwapLittleToHostInt16(ed->wMaxPacketSize);
		ep->interval = ed->bInterval;
		dev->pipes[info->nep].pipe = pipe;
		dev->pipes[info->nep].in = (ed->bEndpointAddress & 0x80) != 0;
		info->nep++;
	}
	return dev;
}

extern "C" void
pdusb_close(void *devp)
{
	struct pdusb_dev *dev = (struct pdusb_dev *)devp;

	if (dev == NULL)
		return;
	dev->dying = true;
	for (int i = 0; i < dev->info.nep; i++)
		pdusb_abort(dev, i);
	IOLockFree(dev->control_lock);
	IOFree(dev, sizeof(*dev));
}

extern "C" int
pdusb_get_info(void *devp, struct pdusb_info *info)
{
	*info = ((struct pdusb_dev *)devp)->info;
	return 0;
}

static int
map_status(IOReturn status)
{
	// the usb status codes are unsigned, so they cannot be case labels on an IOReturn
	if (status == kIOReturnSuccess)
		return PDUSB_OK;
	if (status == kIOUSBPipeStalled)
		return PDUSB_STALLED;
	if (status == kIOReturnAborted)
		return PDUSB_CANCELLED;
	if (status == kIOUSBTransactionTimeout || status == kIOReturnTimeout)
		return PDUSB_TIMEOUT;
	return PDUSB_ERROR;
}

extern "C" int
pdusb_control(void *devp, uint8_t type, uint8_t request, uint16_t value,
	uint16_t index, uint16_t length, void *data, size_t *actual)
{
	struct pdusb_dev *dev = (struct pdusb_dev *)devp;
	IOUSBDevRequest r;

	if (actual != NULL)
		*actual = 0;
	if (dev->dying)
		return PDUSB_ERROR;
	r.bmRequestType = type;
	r.bRequest = request;
	r.wValue = value;
	r.wIndex = index;
	r.wLength = length;
	r.pData = data;
	r.wLenDone = 0;
	IOLockLock(dev->control_lock);
	IOReturn ret = dev->device->DeviceRequest(&r);
	IOLockUnlock(dev->control_lock);
	if (actual != NULL)
		*actual = r.wLenDone;
	return map_status(ret);
}

// runs on the usb thread, the haiku side only queues a task from here
static void
xfer_done(void *target, void *parameter, IOReturn status, UInt32 remaining)
{
	struct pdusb_xfer *x = (struct pdusb_xfer *)target;
	struct pdusb_pipe *pipe = x->pipe;
	size_t actual = remaining > x->len ? 0 : x->len - remaining;

	if (status == kIOReturnSuccess && pipe->in && actual > 0)
		memcpy(x->buf, x->mem->getBytesNoCopy(), actual);
	pdusb_done_t done = x->done;
	void *cookie = x->cookie;
	OSSafeReleaseNULL(x->mem);
	IOFree(x, sizeof(*x));
	done(cookie, map_status(status), actual);
	OSDecrementAtomic(&pipe->inflight);
}

extern "C" int
pdusb_queue(void *devp, int ep, void *buf, size_t len, pdusb_done_t done, void *cookie)
{
	struct pdusb_dev *dev = (struct pdusb_dev *)devp;

	if (dev->dying || ep < 0 || ep >= dev->info.nep)
		return PDUSB_ERROR;
	struct pdusb_pipe *pipe = &dev->pipes[ep];
	struct pdusb_xfer *x = (struct pdusb_xfer *)IOMallocZero(sizeof(*x));
	if (x == NULL)
		return PDUSB_ERROR;
	// a zero-length packet still needs a descriptor to point at
	x->mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
		pipe->in ? kIODirectionIn : kIODirectionOut, len > 0 ? len : 1);
	if (x->mem == NULL) {
		IOFree(x, sizeof(*x));
		return PDUSB_ERROR;
	}
	x->dev = dev;
	x->pipe = pipe;
	x->buf = buf;
	x->len = len;
	x->done = done;
	x->cookie = cookie;
	x->completion.target = x;
	x->completion.action = &xfer_done;
	x->completion.parameter = NULL;
	if (!pipe->in && len > 0)
		memcpy(x->mem->getBytesNoCopy(), buf, len);

	OSIncrementAtomic(&pipe->inflight);
	IOReturn ret = pipe->in
		? pipe->pipe->Read(x->mem, 0, 0, (UInt32)len, &x->completion)
		: pipe->pipe->Write(x->mem, 0, 0, (UInt32)len, &x->completion);
	if (ret != kIOReturnSuccess) {
		OSDecrementAtomic(&pipe->inflight);
		OSSafeReleaseNULL(x->mem);
		IOFree(x, sizeof(*x));
		return map_status(ret);
	}
	return PDUSB_OK;
}

extern "C" int
pdusb_abort(void *devp, int ep)
{
	struct pdusb_dev *dev = (struct pdusb_dev *)devp;

	if (ep < 0 || ep >= dev->info.nep)
		return PDUSB_ERROR;
	struct pdusb_pipe *pipe = &dev->pipes[ep];
	pipe->pipe->Abort();
	// the driver frees its buffers once this returns, so every completion has to have landed
	for (int i = 0; i < 2000 && pipe->inflight > 0; i++)
		IOSleep(1);
	if (pipe->inflight > 0)
		IOLog("pdusb: endpoint 0x%02x still has %d transfers after abort\n",
			dev->info.ep[ep].addr, (int)pipe->inflight);
	pipe->pipe->ClearPipeStall(true);
	return PDUSB_OK;
}
