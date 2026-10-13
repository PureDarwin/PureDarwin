#include "PDUSBHIDDevice.h"

#include <IOKit/IOLib.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <kern/thread.h>

#define super IOHIDDevice
OSDefineMetaClassAndStructors(PDUSBHIDDevice, IOHIDDevice);

// PureDarwin's IOUSBFamily leaves GetDevice/GetInterfaceNumber/GetLocationID unimplemented
// (the kernel collection binds them to panic): read what the nubs publish instead
static UInt32
pdNumberProperty(const IORegistryEntry *entry, const char *key)
{
	OSNumber *n = entry ? OSDynamicCast(OSNumber, entry->getProperty(key)) : NULL;

	return n ? n->unsigned32BitValue() : 0;
}

enum {
	kHIDDescriptorReport = 0x22,
	kHIDRequestGetReport = 0x01,
	kHIDRequestSetReport = 0x09,
	kHIDRequestSetIdle = 0x0a,
	kHIDRequestSetProtocol = 0x0b,
	kHIDMaxReportDescriptor = 4096,
};

// HID 1.11: a six-byte header followed by bNumDescriptors three-byte entries.
static inline uint16_t
pdUSBHIDReportDescriptorLength(const uint8_t *bytes, size_t length)
{
    if (!bytes || length < 6 || bytes[1] != 0x21 || bytes[0] < 6 || bytes[0] > length)
        return 0;
    size_t end = bytes[0];
    if ((size_t)bytes[5] > (end - 6) / 3)
        return 0;
    for (size_t i = 0; i < bytes[5]; ++i) {
        size_t off = 6 + 3 * i;
        if (bytes[off] == 0x22)
            return (uint16_t)(bytes[off + 1] | ((uint16_t)bytes[off + 2] << 8));
    }
    return 0;
}


IOReturn
PDUSBHIDDevice::classRequest(bool in, UInt8 request, UInt16 value, void *data, UInt16 length, UInt32 *done)
{
	IOUSBDevRequest req;
	IOReturn kr;

	bzero(&req, sizeof(req));
	req.bmRequestType = in ? 0xa1 : 0x21;	// class, interface
	req.bRequest = request;
	req.wValue = value;
	req.wIndex = (UInt16)pdNumberProperty(fInterface, "bInterfaceNumber");
	req.wLength = length;
	req.pData = data;
	kr = fInterface->DeviceRequest(&req);
	if (done != NULL) {
		*done = req.wLenDone;
	}
	return kr;
}

bool
PDUSBHIDDevice::handleStart(IOService *provider)
{
    IOUSBFindEndpointRequest pipeReq;
    IOUSBDevRequest req;
    uint8_t hid[255];
    UInt16 requested = kHIDMaxReportDescriptor;
    void *buf = NULL;
    thread_t th;
    IOUSBInterface *interface = OSDynamicCast(IOUSBInterface, provider);

    if (!super::handleStart(provider)) return false;
    setProperty("HIDDefaultBehavior", kOSBooleanTrue);
    if (!interface || !interface->open(this)) {
        IOLog("PDUSBHIDDevice: cannot open the interface\n");
        return false;
    }
    fInterface = interface;
    fInterface->retain();

    // a boot interface may have been left in boot protocol by firmware. The
    // report descriptor describes report protocol, including report IDs/NKRO.
    if (pdNumberProperty(fInterface, "bInterfaceSubClass") == 1 &&
        classRequest(false, kHIDRequestSetProtocol, 1, NULL, 0, NULL) != kIOReturnSuccess) {
        IOLog("PDUSBHIDDevice: cannot select report protocol\n");
        goto fail;
    }
    bzero(&req, sizeof(req));
    bzero(hid, sizeof(hid));
    req.bmRequestType = 0x81;
    req.bRequest = 6;
    req.wValue = 0x21 << 8;
    req.wIndex = (UInt16)pdNumberProperty(fInterface, "bInterfaceNumber");
    req.wLength = 9;
    req.pData = hid;
    if (fInterface->DeviceRequest(&req) == kIOReturnSuccess && req.wLenDone >= 6) {
        if (hid[0] > req.wLenDone) {
            req.wLength = hid[0];
            req.wLenDone = 0;
            if (fInterface->DeviceRequest(&req) != kIOReturnSuccess)
                goto fail;
        }
        requested = pdUSBHIDReportDescriptorLength(hid, req.wLenDone);
        if (!requested || requested > kHIDMaxReportDescriptor)
            goto fail;
    }
    // QEMU devices may not implement GET_DESCRIPTOR(HID). The controller's
    // actual short-transfer length still bounds this fallback report read.
    buf = IOMalloc(requested);
    if (!buf) goto fail;
    bzero(buf, requested);
    bzero(&req, sizeof(req));
    req.bmRequestType = 0x81;
    req.bRequest = 6;
    req.wValue = kHIDDescriptorReport << 8;
    req.wIndex = (UInt16)pdNumberProperty(fInterface, "bInterfaceNumber");
    req.wLength = requested;
    req.pData = buf;
    if (fInterface->DeviceRequest(&req) != kIOReturnSuccess ||
        !req.wLenDone || req.wLenDone > requested) {
        IOLog("PDUSBHIDDevice: no valid report descriptor\n");
        goto fail;
    }
    fDescriptor = OSData::withBytes(buf, req.wLenDone);
    IOFree(buf, requested);
    buf = NULL;
    if (!fDescriptor) goto fail;

    bzero(&pipeReq, sizeof(pipeReq));
    pipeReq.type = kUSBInterrupt;
    pipeReq.direction = kUSBIn;
    fInPipe = fInterface->FindNextPipe(NULL, &pipeReq, true);
    if (!fInPipe) {
        IOLog("PDUSBHIDDevice: no interrupt-IN pipe\n");
        goto fail;
    }
    fMaxPacket = (fInPipe->GetMaxPacketSize() & 0x7ff) *
        (1 + ((fInPipe->GetMaxPacketSize() >> 11) & 3));
    if (!fMaxPacket || fMaxPacket > 3072) goto fail;
    fReadBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionIn, fMaxPacket);
    if (!fReadBuffer) goto fail;

    classRequest(false, kHIDRequestSetIdle, 0, NULL, 0, NULL);
    fRunning = true;
    fPolling = true;
    retain();
    if (kernel_thread_start(&PDUSBHIDDevice::pollThread, this, &th) != KERN_SUCCESS) {
        fPolling = false;
        release();
        goto fail;
    }
    thread_deallocate(th);
    IOLog("PDUSBHIDDevice: USB %04x:%04x interface %u descriptor %u bytes endpoint %u packet %u\n",
        (unsigned)pdNumberProperty(fInterface->getProvider(), "idVendor"),
        (unsigned)pdNumberProperty(fInterface->getProvider(), "idProduct"),
        (unsigned)pdNumberProperty(fInterface, "bInterfaceNumber"),
        (unsigned)fDescriptor->getLength(), fInPipe->GetEndpointNumber(), fMaxPacket);
    return true;
fail:
    if (buf) IOFree(buf, requested);
    fRunning = false;
    fInterface->close(this);
    return false;
}

void
PDUSBHIDDevice::handleStop(IOService *provider)
{
	fRunning = false;
	// reads poll for at most 20 ms. Let the reader drop its buffer access before
	// aborting the controller's persistent TD and closing the provider.
	while (fPolling)
		IOSleep(1);
	if (fInPipe != NULL) {
		fInPipe->Abort();
	}
	if (fInterface != NULL && fInterface->isOpen(this)) {
		fInterface->close(this);
	}
	super::handleStop(provider);
}

void
PDUSBHIDDevice::free(void)
{
	OSSafeReleaseNULL(fReadBuffer);
	OSSafeReleaseNULL(fInPipe);
	OSSafeReleaseNULL(fDescriptor);
	OSSafeReleaseNULL(fInterface);
	super::free();
}

void
PDUSBHIDDevice::pollThread(void *arg, wait_result_t wr)
{
	PDUSBHIDDevice *self = (PDUSBHIDDevice *)arg;

	(void)wr;
	self->pollLoop();
	self->fPolling = false;
	self->release();
	thread_terminate(current_thread());
}

void
PDUSBHIDDevice::pollLoop(void)
{
	while (fRunning) {
		IOByteCount done = fMaxPacket;
		IOReturn kr = fInPipe->Read(fReadBuffer, 0, 0, fMaxPacket, (IOUSBCompletion *)NULL, &done);

		if (kr == kIOReturnSuccess && done > 0 && done <= fMaxPacket && fRunning) {
			IOBufferMemoryDescriptor *report =
			    IOBufferMemoryDescriptor::withBytes(fReadBuffer->getBytesNoCopy(), done, kIODirectionNone);

			if (report != NULL) {
				handleReport(report, kIOHIDReportTypeInput);
				report->release();
			}
		} else if (kr == kIOReturnNotAttached || kr == kIOReturnNoDevice) {
			break;
		} else if (kr == kIOUSBPipeStalled) {
			fInPipe->ClearPipeStall(true);
			IOSleep(10);
		} else if (kr != kIOReturnSuccess) {
			IOSleep(10);
		}
	}
}

IOReturn
PDUSBHIDDevice::newReportDescriptor(IOMemoryDescriptor **descriptor) const
{
	IOBufferMemoryDescriptor *md;

	if (fDescriptor == NULL) {
		return kIOReturnNotReady;
	}
	md = IOBufferMemoryDescriptor::withBytes(fDescriptor->getBytesNoCopy(), fDescriptor->getLength(),
	    kIODirectionNone);
	if (md == NULL) {
		return kIOReturnNoMemory;
	}
	*descriptor = md;
	return kIOReturnSuccess;
}

OSString *
PDUSBHIDDevice::newTransportString() const
{
	return OSString::withCString("USB");
}

OSNumber *
PDUSBHIDDevice::newVendorIDNumber() const
{
	IORegistryEntry *dev = fInterface ? fInterface->getProvider() : NULL;

	return dev ? OSNumber::withNumber(pdNumberProperty(dev, "idVendor"), 32) : NULL;
}

OSNumber *
PDUSBHIDDevice::newProductIDNumber() const
{
	IORegistryEntry *dev = fInterface ? fInterface->getProvider() : NULL;

	return dev ? OSNumber::withNumber(pdNumberProperty(dev, "idProduct"), 32) : NULL;
}

OSNumber *
PDUSBHIDDevice::newLocationIDNumber() const
{
	IORegistryEntry *dev = fInterface ? fInterface->getProvider() : NULL;

	return dev ? OSNumber::withNumber(pdNumberProperty(dev, "locationID"), 32) : NULL;
}

// Report type: input 1, output 2, feature 3 in the request's high byte, the ID in the low byte
IOReturn
PDUSBHIDDevice::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
	if (!report || report->getLength() > 0xffff || reportType > kIOHIDReportTypeFeature)
		return kIOReturnBadArgument;
	UInt32 done = 0, len = (UInt32)report->getLength();
	void *buf = IOMalloc(len ? len : 1);
	IOReturn kr;

	if (buf == NULL) {
		return kIOReturnNoMemory;
	}
	kr = classRequest(true, kHIDRequestGetReport, (UInt16)(((reportType + 1) << 8) | (options & 0xff)), buf,
	    (UInt16)len, &done);
	if (kr == kIOReturnSuccess) {
		if (done > len || report->writeBytes(0, buf, done) != done)
			kr = kIOReturnOverrun;
	}
	IOFree(buf, len ? len : 1);
	return kr;
}

IOReturn
PDUSBHIDDevice::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
	if (!report || report->getLength() > 0xffff || reportType > kIOHIDReportTypeFeature)
		return kIOReturnBadArgument;
	UInt32 len = (UInt32)report->getLength();
	void *buf = IOMalloc(len ? len : 1);
	IOReturn kr;

	if (buf == NULL) {
		return kIOReturnNoMemory;
	}
	report->readBytes(0, buf, len);
	kr = classRequest(false, kHIDRequestSetReport, (UInt16)(((reportType + 1) << 8) | (options & 0xff)), buf,
	    (UInt16)len, NULL);
	IOFree(buf, len ? len : 1);
	return kr;
}

bool
PDUSBHIDDevice::willTerminate(IOService *provider, IOOptionBits options)
{
	fRunning = false;
	if (fInPipe) {
	    fInPipe->Abort();
	}
	return super::willTerminate(provider, options);
}

bool
PDUSBHIDDevice::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
	if (fInterface && fInterface->isOpen(this)) {
	    fInterface->close(this);
	}
	return super::didTerminate(provider, options, defer);
}
