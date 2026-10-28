#include "RavynXHCIUSBBus.h"
#include "RavynXHCIPort.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>

#define super IOUSBController

OSDefineMetaClassAndStructors(RavynXHCIUSBBus, IOUSBController)

bool RavynXHCIUSBBus::initWithPort(RavynXHCIPort *port)
{
    if (!init(NULL)) return false;
    fPort = port;
    return true;
}

IOReturn RavynXHCIUSBBus::UIMOpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !endpoint) return kIOReturnBadArgument;

    switch (endpoint->transferType) {
        case kUSBControl:
            // endpoint 0 is set up by addressDevice() at slot address time
            return kIOReturnSuccess;

        case kUSBBulk:
            return fPort->openBulkEndpoint(slotId, endpoint->number, endpoint->direction == kUSBIn,
                                           endpoint->maxPacketSize) ? kIOReturnSuccess : kIOReturnError;

        case kUSBInterrupt:
            if (endpoint->direction != kUSBIn)
                return kIOReturnUnsupported; // no interrupt-OUT support in the UIM
            if (!fPort->configureInterruptInEndpoint(slotId, endpoint->number,
                                                       endpoint->maxPacketSize, endpoint->interval))
                return kIOReturnError;
            return kIOReturnSuccess;

        default:
            return kIOReturnUnsupported; // isoc: no UIM support
    }
}

IOReturn RavynXHCIUSBBus::UIMClosePipe(USBDeviceAddress address, Endpoint *endpoint)
{
    // no per-endpoint teardown short of tearing down the slot, so this is a no-op that leaves sibling endpoints alone
    return kIOReturnSuccess;
}

IOReturn RavynXHCIUSBBus::UIMAbortPipe(USBDeviceAddress address, Endpoint *endpoint)
{
    if (endpoint && endpoint->transferType == kUSBInterrupt && endpoint->direction == kUSBIn)
        return fPort->abortInterruptEndpoint(address, endpoint->number);
    if (endpoint && endpoint->transferType == kUSBBulk)
        return fPort->abortBulk(address, endpoint->number, endpoint->direction == kUSBIn, false);
    return kIOReturnSuccess;
}

IOReturn RavynXHCIUSBBus::UIMClearPipeStall(USBDeviceAddress address, Endpoint *endpoint)
{
    if (endpoint && endpoint->transferType == kUSBInterrupt && endpoint->direction == kUSBIn)
        return fPort->abortInterruptEndpoint(address, endpoint->number, true);
    if (endpoint && endpoint->transferType == kUSBBulk)
        return fPort->abortBulk(address, endpoint->number, endpoint->direction == kUSBIn, true);
    return kIOReturnSuccess;
}

IOReturn RavynXHCIUSBBus::UIMDeviceRequest(IOUSBDevRequest *request, USBDeviceAddress address)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !request) return kIOReturnBadArgument;

    USBSetupPacket setup;
    setup.bmRequestType = request->bmRequestType;
    setup.bRequest      = request->bRequest;
    setup.wValue        = request->wValue;
    setup.wIndex        = request->wIndex;
    setup.wLength        = request->wLength;

    bool in = (request->bmRequestType & 0x80) != 0;
    request->wLenDone = 0;
    bool ok = fPort->controlTransfer(slotId, setup, request->pData, request->wLength, in, &request->wLenDone);
    return ok ? kIOReturnSuccess : kIOReturnError;
}

IOReturn RavynXHCIUSBBus::UIMReadWrite(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                                       Endpoint *endpoint, bool isWrite)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !endpoint || !buffer) return kIOReturnBadArgument;
    if (endpoint->transferType != kUSBBulk && endpoint->transferType != kUSBInterrupt)
        return kIOReturnUnsupported;

    UInt32 len = (UInt32)buffer->getLength();
    if (!len) return kIOReturnSuccess;

    if (endpoint->transferType == kUSBInterrupt) {
        if (isWrite)
            return kIOReturnUnsupported;
        return fPort->interruptTransfer(slotId, endpoint->number, buffer, len, 20);
    }

    return fPort->bulkSync(slotId, endpoint->number, !isWrite, buffer, len, 5000, NULL);
}

IOReturn RavynXHCIUSBBus::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                              Endpoint *endpoint, IOUSBCompletion *completion)
{
    return Read(buffer, address, endpoint, completion, 0, 0, buffer ? buffer->getLength() : 0);
}

IOReturn RavynXHCIUSBBus::Read(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                              Endpoint *endpoint, IOUSBCompletion *completion,
                              UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    if (endpoint && endpoint->transferType == kUSBBulk)
        return bulkIO(buffer, address, endpoint, completion, completionTimeout, reqCount, true);
    if (!endpoint || endpoint->transferType != kUSBInterrupt)
        return super::Read(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
    UInt32 done = 0;
    IOReturn ret = !buffer || reqCount > buffer->getLength() || reqCount > 0xffffffffULL
        ? kIOReturnBadArgument
        : fPort->interruptTransfer(address, endpoint->number, buffer, (UInt32)reqCount, 20, &done);
    if (completion && completion->action == &IOUSBSyncCompletion) {
        // the reconstructed base family's synchronous completion is a marker
        // IOUSBPipe set this IOByteCount to reqCount, so report the real length
        if (completion->parameter)
            *(IOByteCount *)completion->parameter = done;
    } else if (completion && completion->action) {
        (*completion->action)(completion->target, completion->parameter, ret,
            (UInt32)(reqCount - done));
    }
    return ret;
}

IOReturn RavynXHCIUSBBus::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                               Endpoint *endpoint, IOUSBCompletion *completion)
{
    return Write(buffer, address, endpoint, completion, 0, 0, buffer ? buffer->getLength() : 0);
}

IOReturn RavynXHCIUSBBus::Write(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                               Endpoint *endpoint, IOUSBCompletion *completion,
                               UInt32 noDataTimeout, UInt32 completionTimeout, IOByteCount reqCount)
{
    if (endpoint && endpoint->transferType == kUSBBulk)
        return bulkIO(buffer, address, endpoint, completion, completionTimeout, reqCount, false);
    return super::Write(buffer, address, endpoint, completion, noDataTimeout, completionTimeout, reqCount);
}

// sync callers get the length through the marker's parameter, async ones through the completion
IOReturn RavynXHCIUSBBus::bulkIO(IOMemoryDescriptor *buffer, USBDeviceAddress address, Endpoint *endpoint,
                                IOUSBCompletion *completion, UInt32 timeoutMs, IOByteCount reqCount, bool in)
{
    if (!buffer || !reqCount || reqCount > buffer->getLength() || reqCount > 0xffffffffULL)
        return kIOReturnBadArgument;
    if (completion && completion->action && completion->action != &IOUSBSyncCompletion)
        return fPort->submitBulk(address, endpoint->number, in, buffer, (UInt32)reqCount, completion);

    UInt32 done = 0;
    IOReturn ret = fPort->bulkSync(address, endpoint->number, in, buffer, (UInt32)reqCount,
                                   timeoutMs ? timeoutMs : 5000, &done);
    if (completion && completion->parameter)
        *(IOByteCount *)completion->parameter = done;
    return ret;
}
