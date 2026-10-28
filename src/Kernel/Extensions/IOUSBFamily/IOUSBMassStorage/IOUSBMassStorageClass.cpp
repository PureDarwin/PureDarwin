/*
 * IOUSBMassStorageClass: bulk-only transport over IOUSBPipe.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/usb/USB.h>
#include "IOUSBMassStorageClass.h"
#include "IOUSBMassStorageDisk.h"

#define super IOService
OSDefineMetaClassAndStructors(IOUSBMassStorageClass, IOService);

static void
MSC_Log(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    IOLog("[IOUSBMassStorageClass] %s\n", buf);
}

/* Bulk-only transport wire structures (USB Mass Storage Class, BBB). */
#define kCBWSignature 0x43425355U
#define kCSWSignature 0x53425355U
#define kCBWLength    31
#define kCSWLength    13

#pragma pack(push, 1)
struct BOTCommandBlockWrapper {
    UInt32 dCBWSignature;
    UInt32 dCBWTag;
    UInt32 dCBWDataTransferLength;
    UInt8  bmCBWFlags;
    UInt8  bCBWLUN;
    UInt8  bCBWCBLength;
    UInt8  CBWCB[16];
};

struct BOTCommandStatusWrapper {
    UInt32 dCSWSignature;
    UInt32 dCSWTag;
    UInt32 dCSWDataResidue;
    UInt8  bCSWStatus;
};
#pragma pack(pop)

/* The transfers below are synchronous: passing a NULL completion to IOUSBPipe
 * blocks until the transfer finishes, which is what the block storage path
 * above us already expects, and what makes this usable before the root mount. */
#define kBOTTimeoutMs 5000

bool
IOUSBMassStorageClass::start(IOService *provider)
{
    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if (!fInterface) {
        MSC_Log("provider is not an IOUSBInterface");
        return false;
    }

    if (!super::start(provider))
        return false;

    if (!fInterface->open(this)) {
        MSC_Log("could not open interface");
        return false;
    }

    if (!findBulkPipes()) {
        MSC_Log("no bulk in/out pipe pair on this interface");
        fInterface->close(this);
        return false;
    }

    fTag = 1;
    fBOTLock = IOLockAlloc();

    fDisk = new IOUSBMassStorageDisk;
    if (!fDisk || !fDisk->initWithTransport(this, 0)) {
        MSC_Log("failed to create disk nub");
        if (fDisk) { fDisk->release(); fDisk = NULL; }
        fInterface->close(this);
        return false;
    }

    if (!fDisk->attach(this) || !fDisk->start(this)) {
        MSC_Log("failed to attach disk nub");
        fDisk->release();
        fDisk = NULL;
        fInterface->close(this);
        return false;
    }
    fDisk->registerService();

    MSC_Log("mass storage interface started");
    return true;
}

void
IOUSBMassStorageClass::stop(IOService *provider)
{
    if (fDisk) {
        fDisk->terminate();
        fDisk->release();
        fDisk = NULL;
    }
    if (fInterface)
        fInterface->close(this);
    super::stop(provider);
}

void
IOUSBMassStorageClass::free()
{
    fBulkIn = NULL;
    fBulkOut = NULL;
    fInterface = NULL;
    if (fBOTLock) {
        IOLockFree(fBOTLock);
        fBOTLock = NULL;
    }
    super::free();
}

bool
IOUSBMassStorageClass::findBulkPipes(void)
{
    IOUSBFindEndpointRequest req;

    bzero(&req, sizeof(req));
    req.type = kUSBBulk;
    req.direction = kUSBIn;
    fBulkIn = fInterface->FindNextPipe(NULL, &req);

    bzero(&req, sizeof(req));
    req.type = kUSBBulk;
    req.direction = kUSBOut;
    fBulkOut = fInterface->FindNextPipe(NULL, &req);

    return (fBulkIn != NULL) && (fBulkOut != NULL);
}

IOReturn
IOUSBMassStorageClass::sendCBW(const void *cb, UInt8 cbLen, UInt32 dataLen, bool dataIn)
{
    if (cbLen > 16)
        return kIOReturnBadArgument;

    IOBufferMemoryDescriptor *cbwMem = IOBufferMemoryDescriptor::inTaskWithOptions(
        kernel_task, kIODirectionOut, kCBWLength);
    if (!cbwMem)
        return kIOReturnNoMemory;

    BOTCommandBlockWrapper *cbw = (BOTCommandBlockWrapper *)cbwMem->getBytesNoCopy();
    bzero(cbw, kCBWLength);
    cbw->dCBWSignature = OSSwapHostToLittleInt32(kCBWSignature);
    cbw->dCBWTag = OSSwapHostToLittleInt32(fTag);
    cbw->dCBWDataTransferLength = OSSwapHostToLittleInt32(dataLen);
    cbw->bmCBWFlags = dataIn ? 0x80 : 0x00;
    cbw->bCBWLUN = 0;
    cbw->bCBWCBLength = cbLen;
    bcopy(cb, cbw->CBWCB, cbLen);

    IOReturn ret = fBulkOut->Write(cbwMem, kBOTTimeoutMs, kBOTTimeoutMs,
                                   kCBWLength, NULL);
    cbwMem->release();
    return ret;
}

IOReturn
IOUSBMassStorageClass::readCSW(UInt32 *residueOut, UInt8 *statusOut)
{
    IOBufferMemoryDescriptor *cswMem = IOBufferMemoryDescriptor::inTaskWithOptions(
        kernel_task, kIODirectionIn, kCSWLength);
    if (!cswMem)
        return kIOReturnNoMemory;
    bzero(cswMem->getBytesNoCopy(), kCSWLength);

    IOByteCount got = 0;
    IOReturn ret = fBulkIn->Read(cswMem, kBOTTimeoutMs, kBOTTimeoutMs,
                                 kCSWLength, (IOUSBCompletion *)NULL, &got);

    /* A stalled bulk-in after the data stage is the specified way a device
     * reports it had less to say than we asked for; clearing it and retrying
     * once is what gets the CSW out. */
    if (ret == kIOUSBPipeStalled) {
        fBulkIn->ClearPipeStall(true);
        got = 0;
        ret = fBulkIn->Read(cswMem, kBOTTimeoutMs, kBOTTimeoutMs,
                            kCSWLength, (IOUSBCompletion *)NULL, &got);
    }

    if (ret == kIOReturnSuccess) {
        const BOTCommandStatusWrapper *csw =
            (const BOTCommandStatusWrapper *)cswMem->getBytesNoCopy();
        if (got != kCSWLength ||
            OSSwapLittleToHostInt32(csw->dCSWSignature) != kCSWSignature ||
            OSSwapLittleToHostInt32(csw->dCSWTag) != fTag) {
            MSC_Log("bad CSW (got=%llu sig=%08x tag=%08x expected tag=%08x)",
                    (UInt64)got, OSSwapLittleToHostInt32(csw->dCSWSignature),
                    OSSwapLittleToHostInt32(csw->dCSWTag), fTag);
            ret = kIOReturnIOError;
        } else {
            if (residueOut) *residueOut = OSSwapLittleToHostInt32(csw->dCSWDataResidue);
            if (statusOut) *statusOut = csw->bCSWStatus;
        }
    }

    cswMem->release();
    return ret;
}

IOReturn
IOUSBMassStorageClass::botTransfer(const void *cb, UInt8 cbLen,
                                   UInt32 dataLen, bool dataIn,
                                   IOMemoryDescriptor *buffer, UInt64 bufOff)
{
    // command, data and status of one command must not interleave with another's
    if (fBOTLock)
        IOLockLock(fBOTLock);
    IOReturn ret = botTransferLocked(cb, cbLen, dataLen, dataIn, buffer, bufOff);
    if (fBOTLock)
        IOLockUnlock(fBOTLock);
    return ret;
}

IOReturn
IOUSBMassStorageClass::botTransferLocked(const void *cb, UInt8 cbLen,
                                         UInt32 dataLen, bool dataIn,
                                         IOMemoryDescriptor *buffer, UInt64 bufOff)
{
    if (!fBulkIn || !fBulkOut || !cb || !cbLen)
        return kIOReturnBadArgument;
    if (dataLen && !buffer)
        return kIOReturnBadArgument;

    fTag++;

    IOReturn ret = sendCBW(cb, cbLen, dataLen, dataIn);
    if (ret != kIOReturnSuccess) {
        MSC_Log("CBW failed (%08x)", ret);
        return ret;
    }

    IOByteCount moved = 0;
    if (dataLen) {
        /* Only wrap when the command addresses part of a larger buffer; the
         * common whole-buffer case avoids the extra descriptor entirely. */
        IOMemoryDescriptor *data = buffer;
        IOSubMemoryDescriptor *sub = NULL;
        if (bufOff != 0 || dataLen != buffer->getLength()) {
            sub = IOSubMemoryDescriptor::withSubRange(
                buffer, bufOff, dataLen,
                dataIn ? kIODirectionIn : kIODirectionOut);
            if (!sub)
                return kIOReturnNoMemory;
            data = sub;
        }

        if (dataIn)
            ret = fBulkIn->Read(data, kBOTTimeoutMs, kBOTTimeoutMs, dataLen, (IOUSBCompletion *)NULL, &moved);
        else
            ret = fBulkOut->Write(data, kBOTTimeoutMs, kBOTTimeoutMs, dataLen, NULL);

        if (sub)
            sub->release();

        /* A stall in the data stage aborts just that stage - the CSW still has
         * to be collected or the device stays out of step with us. */
        if (ret == kIOUSBPipeStalled) {
            (dataIn ? fBulkIn : fBulkOut)->ClearPipeStall(true);
        } else if (ret != kIOReturnSuccess) {
            MSC_Log("data stage failed (%08x)", ret);
            return ret;
        }
    }

    UInt32 residue = 0;
    UInt8 status = 0;
    ret = readCSW(&residue, &status);
    if (ret != kIOReturnSuccess)
        return ret;

    if (status != 0) {
        MSC_Log("command failed, CSW status %u residue %u", status, residue);
        return kIOReturnIOError;
    }
    if (dataLen && residue) {
        MSC_Log("short transfer: asked %u, residue %u", dataLen, residue);
        return kIOReturnUnderrun;
    }

    return kIOReturnSuccess;
}
