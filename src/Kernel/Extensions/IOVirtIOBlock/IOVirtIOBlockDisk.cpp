#include "IOVirtIOBlockDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(IOVirtIOBlockDisk, IOBlockStorageDevice);

bool
IOVirtIOBlockDisk::initWithController(IOVirtIOBlock *controller)
{
    if (!controller) return false;
    if (!init(NULL)) return false;

    fController = controller;
    setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
    setName("virtio-blk0");
    setLocation("0");
    return true;
}

bool IOVirtIOBlockDisk::start(IOService *provider)
{
    return super::start(provider);
}

void IOVirtIOBlockDisk::stop(IOService *provider)
{
    super::stop(provider);
}

void IOVirtIOBlockDisk::free()
{
    fController = NULL;
    super::free();
}

IOReturn
IOVirtIOBlockDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                                    UInt64 block,
                                    UInt64 nblks,
                                    IOStorageAttributes *attributes,
                                    IOStorageCompletion *completion)
{
    (void)attributes;
    if (!fController || !buffer) {
        IOStorage::complete(completion, kIOReturnBadArgument, 0);
        return kIOReturnBadArgument;
    }

    bool write = (buffer->getDirection() & kIODirectionOut) != 0;
    IOReturn prep = buffer->prepare();
    if (prep != kIOReturnSuccess) {
        IOStorage::complete(completion, prep, 0);
        return prep;
    }

    IOReturn ret = fController->readWrite(write, block, nblks, buffer);
    buffer->complete();
    IOStorage::complete(completion, ret,
        ret == kIOReturnSuccess ? nblks * fController->blockSize() : 0);
    return ret;
}

IOReturn
IOVirtIOBlockDisk::doSynchronize(UInt64 block,
                                 UInt64 nblks,
                                 IOStorageSynchronizeOptions options)
{
    (void)block;
    (void)nblks;
    (void)options;
    return fController ? fController->flush() : kIOReturnNoDevice;
}

IOReturn IOVirtIOBlockDisk::doEjectMedia(void) { return kIOReturnUnsupported; }

IOReturn IOVirtIOBlockDisk::doFormatMedia(UInt64 byteCapacity)
{
    (void)byteCapacity;
    return kIOReturnUnsupported;
}

UInt32
IOVirtIOBlockDisk::doGetFormatCapacities(UInt64 *capacities,
                                         UInt32 capacitiesMaxCount) const
{
    if (capacities && capacitiesMaxCount && fController)
        capacities[0] = fController->blockCount() * fController->blockSize();
    return 1;
}

char *IOVirtIOBlockDisk::getVendorString(void)   { return (char *)"VirtIO"; }
char *IOVirtIOBlockDisk::getProductString(void)  { return (char *)"Block Device"; }
char *IOVirtIOBlockDisk::getRevisionString(void) { return (char *)"1.0"; }
char *IOVirtIOBlockDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn IOVirtIOBlockDisk::reportBlockSize(UInt64 *blockSize)
{
    if (!blockSize || !fController) return kIOReturnBadArgument;
    *blockSize = fController->blockSize();
    return kIOReturnSuccess;
}

IOReturn IOVirtIOBlockDisk::reportEjectability(bool *isEjectable)
{
    if (isEjectable) *isEjectable = false;
    return kIOReturnSuccess;
}

IOReturn IOVirtIOBlockDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
    if (!maxBlock || !fController) return kIOReturnBadArgument;
    UInt64 blocks = fController->blockCount();
    *maxBlock = blocks ? blocks - 1 : 0;
    return kIOReturnSuccess;
}

IOReturn IOVirtIOBlockDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
    if (mediaPresent) *mediaPresent = fController != NULL;
    if (changedState) *changedState = false;
    return kIOReturnSuccess;
}

IOReturn IOVirtIOBlockDisk::reportRemovability(bool *isRemovable)
{
    if (isRemovable) *isRemovable = false;
    return kIOReturnSuccess;
}

IOReturn IOVirtIOBlockDisk::reportWriteProtection(bool *isWriteProtected)
{
    if (isWriteProtected) *isWriteProtected = fController && fController->isReadOnly();
    return kIOReturnSuccess;
}
