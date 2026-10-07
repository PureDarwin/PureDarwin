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
    bzero(fBulkConfigured, sizeof(fBulkConfigured));
    return true;
}

IOReturn RavynXHCIUSBBus::UIMOpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !endpoint) return kIOReturnBadArgument;

    switch (endpoint->transferType) {
        case kUSBControl:
            /* Endpoint 0 is already set up by addressDevice() at slot
             * address time - nothing to do. */
            return kIOReturnSuccess;

        case kUSBBulk:
            if (fBulkConfigured[slotId])
                return kIOReturnSuccess; /* single ring pair per slot, see header note */
            {
                UInt8 inEp  = (endpoint->direction == kUSBIn)  ? endpoint->number : 0;
                UInt8 outEp = (endpoint->direction == kUSBOut) ? endpoint->number : 0;
                /* We only ever see one direction per OpenPipe call, but
                 * configureBulkEndpoints() wants both at once - the other
                 * side just won't be usable until/unless it's also opened.
                 * Real composite devices with only one bulk direction are
                 * the common case this covers. */
                if (!fPort->configureBulkEndpoints(slotId, inEp, endpoint->maxPacketSize,
                                                    outEp, endpoint->maxPacketSize))
                    return kIOReturnError;
                fBulkConfigured[slotId] = true;
            }
            return kIOReturnSuccess;

        case kUSBInterrupt:
            if (endpoint->direction != kUSBIn)
                return kIOReturnUnsupported; /* no interrupt-OUT support in the UIM */
            if (!fPort->configureInterruptInEndpoint(slotId, endpoint->number,
                                                       endpoint->maxPacketSize, endpoint->interval))
                return kIOReturnError;
            return kIOReturnSuccess;

        default:
            return kIOReturnUnsupported; /* isoc: no UIM support */
    }
}

IOReturn RavynXHCIUSBBus::UIMClosePipe(USBDeviceAddress address, Endpoint *endpoint)
{
    /* No per-endpoint teardown primitive in RavynXHCIPort short of tearing
     * down the whole slot - treat as a no-op rather than disturb sibling
     * endpoints sharing the slot's single bulk/interrupt ring pair. */
    return kIOReturnSuccess;
}

IOReturn RavynXHCIUSBBus::UIMAbortPipe(USBDeviceAddress address, Endpoint *endpoint)
{
    if (endpoint && endpoint->transferType == kUSBInterrupt && endpoint->direction == kUSBIn)
        return fPort->abortInterruptEndpoint(address, endpoint->number);
    return kIOReturnSuccess;
}

IOReturn RavynXHCIUSBBus::UIMClearPipeStall(USBDeviceAddress address, Endpoint *endpoint)
{
    if (endpoint && endpoint->transferType == kUSBInterrupt && endpoint->direction == kUSBIn)
        return fPort->abortInterruptEndpoint(address, endpoint->number, true);
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

    IOBufferMemoryDescriptor *bounce =
        IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut, len);
    if (!bounce) return kIOReturnNoMemory;

    if (isWrite)
        buffer->readBytes(0, bounce->getBytesNoCopy(), len);

    bool ok = fPort->bulkTransfer(slotId, endpoint->number, !isWrite, bounce, len, 5000);

    if (ok && !isWrite)
        buffer->writeBytes(0, bounce->getBytesNoCopy(), len);

    bounce->release();
    return ok ? kIOReturnSuccess : kIOReturnError;
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
