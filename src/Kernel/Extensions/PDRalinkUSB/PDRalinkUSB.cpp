/*
 * PDRalinkUSB.cpp - Ralink RT2870, RT3070, RT3572 and RT5370 USB 802.11 adapters.
 * The radio is OpenBSD's run(4) (ISC licensed, see LICENSE.run), this file
 * attaches it to IOKit and carries out the usbdi calls it makes.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include "IO80211Controller.h"
#include "pd_usbd.h"
#include "pd_run_glue.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBPipe.h>

extern "C" {
void pd80211_lock(void);
void pd80211_unlock(void);
}

#define kMaxEndpoints 16

struct usbd_device {
    IOUSBDevice *dev;
    volatile bool dying;
};

struct usbd_pipe {
    IOUSBPipe *pipe;
    uint8_t addr;
    bool in;
};

struct usbd_interface {
    IOUSBInterface *iface;
    usb_interface_descriptor_t desc;
    usb_endpoint_descriptor_t eps[kMaxEndpoints];
    struct usbd_pipe pipes[kMaxEndpoints];
    uint8_t count;
};

struct usbd_xfer {
    struct usbd_device *dev;
    struct usbd_pipe *pipe;
    IOBufferMemoryDescriptor *mem;
    void *buf;
    uint32_t size, len, actlen;
    uint16_t flags;
    uint32_t timeout;
    usbd_callback cb;
    void *priv;
    usbd_status status;
    IOUSBCompletion completion;
};

class PDRalinkUSB : public IO80211Controller
{
    OSDeclareDefaultStructors(PDRalinkUSB)

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual const OSString *newVendorString() const APPLE_KEXT_OVERRIDE;
    virtual const OSString *newModelString() const APPLE_KEXT_OVERRIDE;

private:
    IOUSBInterface *fInterface;
    struct usbd_device fDevice;
    struct usbd_interface fUSBInterface;
    struct run_softc *fSoftc;
};

#define super IO80211Controller
OSDefineMetaClassAndStructors(PDRalinkUSB, IO80211Controller)

// every bulk endpoint of the interface, as run_attach walks them
static void collectEndpoints(struct usbd_interface *ui)
{
    ui->count = 0;
    // the pipe list is not packed from index 0, so walk all of it
    for (UInt8 i = 0; i < kUSBMaxPipes && ui->count < kMaxEndpoints; i++) {
        IOUSBPipe *pipe = ui->iface->GetPipeObj(i);
        const IOUSBEndpointDescriptor *ed = pipe ? pipe->GetEndpointDescriptor() : NULL;

        if (ed == NULL)
            continue;
        usb_endpoint_descriptor_t *out = &ui->eps[ui->count];
        out->bLength = sizeof(*out);
        out->bDescriptorType = 5;
        out->bEndpointAddress = ed->bEndpointAddress;
        out->bmAttributes = ed->bmAttributes;
        USETW(out->wMaxPacketSize, OSSwapLittleToHostInt16(ed->wMaxPacketSize));
        out->bInterval = ed->bInterval;
        ui->pipes[ui->count].pipe = pipe;
        ui->pipes[ui->count].addr = ed->bEndpointAddress;
        ui->pipes[ui->count].in = (ed->bEndpointAddress & 0x80) != 0;
        IOLog("PDRalinkUSB: endpoint 0x%02x attributes 0x%02x at pipe %u\n", ed->bEndpointAddress, ed->bmAttributes, i);
        ui->count++;
    }
    bzero(&ui->desc, sizeof(ui->desc));
    ui->desc.bLength = sizeof(ui->desc);
    ui->desc.bDescriptorType = 4;
    ui->desc.bInterfaceNumber = ui->iface->GetInterfaceNumber();
    ui->desc.bNumEndpoints = ui->count;
}

bool PDRalinkUSB::start(IOService *provider)
{
    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if (fInterface == NULL || !super::start(provider))
        return false;
    if (!fInterface->open(this)) {
        IOLog("PDRalinkUSB: cannot open the interface\n");
        return false;
    }
    fDevice.dev = fInterface->GetDevice();
    fDevice.dying = false;
    fUSBInterface.iface = fInterface;
    collectEndpoints(&fUSBInterface);
    IOLog("PDRalinkUSB: %u endpoints, the interface reports %u\n", fUSBInterface.count, fInterface->GetNumEndpoints());

    fSoftc = pd_run_alloc(&fDevice, &fUSBInterface, "run0");
    if (fSoftc == NULL || pd_run_attach(fSoftc) != 0) {
        IOLog("PDRalinkUSB: radio did not attach\n");
        fInterface->close(this);
        return false;
    }
    if (!attach80211(pd_run_ic(fSoftc))) {
        pd_run_detach(fSoftc);
        fInterface->close(this);
        return false;
    }
    registerService();
    return true;
}

void PDRalinkUSB::stop(IOService *provider)
{
    fDevice.dying = true;
    detach80211();
    if (fSoftc) {
        pd_run_detach(fSoftc);
        pd_run_free(fSoftc);
        fSoftc = NULL;
    }
    if (fInterface)
        fInterface->close(this);
    super::stop(provider);
}

const OSString *PDRalinkUSB::newVendorString() const
{
    return OSString::withCString("Ralink");
}

const OSString *PDRalinkUSB::newModelString() const
{
    return OSString::withCString("RT2870/RT3070/RT3572 USB");
}

// the usbdi calls run(4) makes

extern "C" {

usb_interface_descriptor_t *usbd_get_interface_descriptor(struct usbd_interface *ui)
{
    return &ui->desc;
}

usb_endpoint_descriptor_t *usbd_interface2endpoint_descriptor(struct usbd_interface *ui, uint8_t i)
{
    return i < ui->count ? &ui->eps[i] : NULL;
}

usbd_status usbd_do_request(struct usbd_device *dev, usb_device_request_t *req, void *data)
{
    IOUSBDevRequest r;

    if (dev->dying)
        return USBD_IOERROR;
    r.bmRequestType = req->bmRequestType;
    r.bRequest = req->bRequest;
    r.wValue = UGETW(req->wValue);
    r.wIndex = UGETW(req->wIndex);
    r.wLength = UGETW(req->wLength);
    r.pData = data;
    r.wLenDone = 0;
    IOReturn ret = dev->dev->DeviceRequest(&r);
    if (ret == kIOUSBPipeStalled)
        return USBD_STALLED;
    return ret == kIOReturnSuccess ? USBD_NORMAL_COMPLETION : USBD_IOERROR;
}

usbd_status usbd_open_pipe(struct usbd_interface *ui, uint8_t addr, uint8_t flags, struct usbd_pipe **out)
{
    for (uint8_t i = 0; i < ui->count; i++) {
        if (ui->pipes[i].addr == addr) {
            *out = &ui->pipes[i];
            return USBD_NORMAL_COMPLETION;
        }
    }
    return USBD_INVAL;
}

void usbd_abort_pipe(struct usbd_pipe *p)
{
    if (p && p->pipe)
        p->pipe->Abort();
}

void usbd_close_pipe(struct usbd_pipe *p)
{
    usbd_abort_pipe(p);
}

void usbd_clear_endpoint_stall_async(struct usbd_pipe *p)
{
    if (p && p->pipe)
        p->pipe->ClearPipeStall(true);
}

struct usbd_xfer *usbd_alloc_xfer(struct usbd_device *dev)
{
    struct usbd_xfer *x = (struct usbd_xfer *)IOMallocZero(sizeof(*x));

    if (x)
        x->dev = dev;
    return x;
}

void usbd_free_xfer(struct usbd_xfer *x)
{
    if (x == NULL)
        return;
    OSSafeReleaseNULL(x->mem);
    IOFree(x, sizeof(*x));
}

void *usbd_alloc_buffer(struct usbd_xfer *x, uint32_t size)
{
    OSSafeReleaseNULL(x->mem);
    x->mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut, size);
    if (x->mem == NULL)
        return NULL;
    x->size = size;
    x->buf = x->mem->getBytesNoCopy();
    return x->buf;
}

void usbd_setup_xfer(struct usbd_xfer *x, struct usbd_pipe *p, void *priv, void *buf, uint32_t len,
                     uint16_t flags, uint32_t timeout, usbd_callback cb)
{
    x->pipe = p;
    x->priv = priv;
    if (buf != x->buf && buf && len <= x->size)
        memcpy(x->buf, buf, len);
    x->len = len;
    x->flags = flags;
    x->timeout = timeout;
    x->cb = cb;
    x->actlen = 0;
}

// the bus completes on its own thread, run's callbacks run under the stack lock like a usb interrupt
static void xferDone(void *target, void *parameter, IOReturn status, UInt32 remaining)
{
    struct usbd_xfer *x = (struct usbd_xfer *)target;

    x->actlen = remaining > x->len ? 0 : x->len - remaining;
    if (status == kIOReturnSuccess)
        x->status = USBD_NORMAL_COMPLETION;
    else if (status == kIOReturnAborted)
        x->status = USBD_CANCELLED;
    else if (status == kIOUSBPipeStalled)
        x->status = USBD_STALLED;
    else if (status == kIOUSBTransactionTimeout)
        x->status = USBD_TIMEOUT;
    else
        x->status = USBD_IOERROR;
    if (x->cb == NULL)
        return;
    pd80211_lock();
    x->cb(x, x->priv, x->status);
    pd80211_unlock();
}

usbd_status usbd_transfer(struct usbd_xfer *x)
{
    IOReturn ret;

    if (x->dev->dying || x->pipe == NULL || x->mem == NULL)
        return USBD_IOERROR;
    x->completion.target = x;
    x->completion.action = &xferDone;
    x->completion.parameter = NULL;
    if (x->pipe->in)
        ret = x->pipe->pipe->Read(x->mem, 0, 0, x->len, &x->completion);
    else
        ret = x->pipe->pipe->Write(x->mem, 0, x->timeout, x->len, &x->completion);
    return ret == kIOReturnSuccess ? USBD_IN_PROGRESS : USBD_IOERROR;
}

void usbd_get_xfer_status(struct usbd_xfer *x, void **priv, void **buf, uint32_t *count, usbd_status *status)
{
    if (priv)
        *priv = x->priv;
    if (buf)
        *buf = x->buf;
    if (count)
        *count = x->actlen;
    if (status)
        *status = x->status;
}

int usbd_is_dying(struct usbd_device *dev)
{
    return dev->dying;
}

void usbd_delay_ms(struct usbd_device *dev, uint32_t ms)
{
    IOSleep(ms);
}

}
