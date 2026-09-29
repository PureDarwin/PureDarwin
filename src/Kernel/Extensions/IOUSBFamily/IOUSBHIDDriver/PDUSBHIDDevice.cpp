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
	kHIDMaxReportDescriptor = 4096,
};

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
	void *buf;
	thread_t th;

	if (!super::handleStart(provider)) {
		return false;
	}
	// IOHIDEventDriver personalities only match transports that publish this
	setProperty("HIDDefaultBehavior", kOSBooleanTrue);
	fInterface = OSDynamicCast(IOUSBInterface, provider);
	if (fInterface == NULL || !fInterface->open(this)) {
		IOLog("PDUSBHIDDevice: cannot open the interface\n");
		return false;
	}

	// GET_DESCRIPTOR(Report) is a standard request to the interface
	buf = IOMalloc(kHIDMaxReportDescriptor);
	if (buf == NULL) {
		return false;
	}
	bzero(&req, sizeof(req));
	req.bmRequestType = 0x81;
	req.bRequest = 6;
	req.wValue = kHIDDescriptorReport << 8;
	req.wIndex = (UInt16)pdNumberProperty(fInterface, "bInterfaceNumber");
	req.wLength = kHIDMaxReportDescriptor;
	req.pData = buf;
	bzero(buf, kHIDMaxReportDescriptor);
	if (fInterface->DeviceRequest(&req) != kIOReturnSuccess) {
		IOLog("PDUSBHIDDevice: no report descriptor\n");
		IOFree(buf, kHIDMaxReportDescriptor);
		return false;
	}
	// wLenDone comes back as the requested size here, and QEMU answers no HID (0x21)
	// descriptor: walk the short items to where the descriptor ends (a 0x00 prefix)
	{
		const UInt8 *d = (const UInt8 *)buf;
		UInt32 off = 0, reportLen;

		while (off < kHIDMaxReportDescriptor && d[off] != 0) {
			UInt8 size = d[off] & 3;

			off += 1 + (size == 3 ? 4 : size);
		}
		reportLen = off > kHIDMaxReportDescriptor ? kHIDMaxReportDescriptor : off;
		if (reportLen == 0) {
			IOFree(buf, kHIDMaxReportDescriptor);
			return false;
		}
		fDescriptor = OSData::withBytes(buf, reportLen);
	}
	IOFree(buf, kHIDMaxReportDescriptor);
	if (fDescriptor == NULL) {
		return false;
	}

	bzero(&pipeReq, sizeof(pipeReq));
	pipeReq.type = kUSBInterrupt;
	pipeReq.direction = kUSBIn;
	fInPipe = fInterface->FindNextPipe(NULL, &pipeReq, true);
	if (fInPipe == NULL) {
		IOLog("PDUSBHIDDevice: no interrupt-IN pipe\n");
		return false;
	}
	fMaxPacket = fInPipe->GetMaxPacketSize();
	if (fMaxPacket == 0 || fMaxPacket > 1024) {
		fMaxPacket = 64;
	}
	fReadBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionIn, fMaxPacket);
	if (fReadBuffer == NULL) {
		return false;
	}

	// Report only on change
	classRequest(false, kHIDRequestSetIdle, 0, NULL, 0, NULL);

	fRunning = true;
	fPolling = true;
	retain();
	if (kernel_thread_start(&PDUSBHIDDevice::pollThread, this, &th) != KERN_SUCCESS) {
		fPolling = false;
		release();
		return false;
	}
	thread_deallocate(th);
	return true;
}

void
PDUSBHIDDevice::handleStop(IOService *provider)
{
	fRunning = false;
	if (fInPipe != NULL) {
		fInPipe->Abort();
	}
	if (fInterface != NULL) {
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

		if (kr == kIOReturnSuccess && done > 0 && fRunning) {
			IOBufferMemoryDescriptor *report =
			    IOBufferMemoryDescriptor::withBytes(fReadBuffer->getBytesNoCopy(), done, kIODirectionNone);

			if (report != NULL) {
				handleReport(report, kIOHIDReportTypeInput);
				report->release();
			}
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
	UInt32 done = 0, len = (UInt32)report->getLength();
	void *buf = IOMalloc(len ? len : 1);
	IOReturn kr;

	if (buf == NULL) {
		return kIOReturnNoMemory;
	}
	kr = classRequest(true, kHIDRequestGetReport, (UInt16)(((reportType + 1) << 8) | (options & 0xff)), buf,
	    (UInt16)len, &done);
	if (kr == kIOReturnSuccess) {
		report->writeBytes(0, buf, done);
	}
	IOFree(buf, len ? len : 1);
	return kr;
}

IOReturn
PDUSBHIDDevice::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
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
