/*
 * IOUSBMassStorageClass: drives a bulk-only transport mass storage interface
 * over IOUSBPipe, and publishes an IOUSBMassStorageDisk for it.
 *
 * This is the transport half for the IOUSBFamily controller stack (OHCI, EHCI,
 * UHCI). All SCSI knowledge lives in IOUSBMassStorageDisk.
 */

#ifndef _IOUSB_MASS_STORAGE_CLASS_H
#define _IOUSB_MASS_STORAGE_CLASS_H

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include "IOUSBBOTTransport.h"

class IOUSBMassStorageDisk;

class IOUSBMassStorageClass: public IOService, public IOUSBBOTTransport
{
    OSDeclareDefaultStructors(IOUSBMassStorageClass);

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    IOReturn botTransfer(const void *cb, UInt8 cbLen,
                         UInt32 dataLen, bool dataIn,
                         IOMemoryDescriptor *buffer, UInt64 bufOff) override;

private:
    IOUSBInterface       *fInterface;
    IOUSBPipe            *fBulkIn;
    IOUSBPipe            *fBulkOut;
    IOUSBMassStorageDisk *fDisk;
    UInt32                fTag;
    IOLock               *fBOTLock;

    bool findBulkPipes(void);
    IOReturn botTransferLocked(const void *cb, UInt8 cbLen, UInt32 dataLen, bool dataIn,
                               IOMemoryDescriptor *buffer, UInt64 bufOff);
    IOReturn sendCBW(const void *cb, UInt8 cbLen, UInt32 dataLen, bool dataIn);
    IOReturn readCSW(UInt32 *residueOut, UInt8 *statusOut);
};

#endif /* _IOUSB_MASS_STORAGE_CLASS_H */
