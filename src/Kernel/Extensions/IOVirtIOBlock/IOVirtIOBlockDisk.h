#ifndef _PD_IOVIRTIO_BLOCK_DISK_H
#define _PD_IOVIRTIO_BLOCK_DISK_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include "IOVirtIOBlock.h"

class IOVirtIOBlockDisk : public IOBlockStorageDevice
{
    OSDeclareDefaultStructors(IOVirtIOBlockDisk);

public:
    bool initWithController(IOVirtIOBlock *controller);
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer,
                              UInt64 block,
                              UInt64 nblks,
                              IOStorageAttributes *attributes,
                              IOStorageCompletion *completion) override;
    IOReturn doSynchronize(UInt64 block,
                           UInt64 nblks,
                           IOStorageSynchronizeOptions options = 0) override;
    IOReturn doEjectMedia(void) override;
    IOReturn doFormatMedia(UInt64 byteCapacity) override;
    UInt32 doGetFormatCapacities(UInt64 *capacities,
                                 UInt32 capacitiesMaxCount) const override;

    char *getVendorString(void) override;
    char *getProductString(void) override;
    char *getRevisionString(void) override;
    char *getAdditionalDeviceInfoString(void) override;

    IOReturn reportBlockSize(UInt64 *blockSize) override;
    IOReturn reportEjectability(bool *isEjectable) override;
    IOReturn reportMaxValidBlock(UInt64 *maxBlock) override;
    IOReturn reportMediaState(bool *mediaPresent,
                              bool *changedState = 0) override;
    IOReturn reportRemovability(bool *isRemovable) override;
    IOReturn reportWriteProtection(bool *isWriteProtected) override;

private:
    IOVirtIOBlock *fController;
};

#endif /* _PD_IOVIRTIO_BLOCK_DISK_H */
