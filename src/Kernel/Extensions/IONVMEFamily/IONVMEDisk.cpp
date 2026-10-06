#include "IONVMEDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(IONVMEDisk, IOBlockStorageDevice);

bool
IONVMEDisk::initWithController(IONVMEController *controller)
{
    if (!controller) return false;
    if (!init(NULL)) return false;

    fController = controller;
    strlcpy(fVendor, "NVMe", sizeof(fVendor));
    strlcpy(fProduct, controller->modelString(), sizeof(fProduct));
    strlcpy(fRevision, controller->firmwareString(), sizeof(fRevision));
    setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
    setName("nvme0");
    setLocation("0");
    return true;
}

bool IONVMEDisk::start(IOService *provider)
{
    return super::start(provider);
}

void IONVMEDisk::stop(IOService *provider)
{
    super::stop(provider);
}

void IONVMEDisk::free()
{
    fController = NULL;
    super::free();
}

IOReturn
IONVMEDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer,
                             UInt64 block,
                             UInt64 nblks,
                             IOStorageAttributes *attributes,
                             IOStorageCompletion *completion)
{
    (void)attributes;
    // an error return means not started and IOBlockStorageDriver completes the request itself:
    // completing here as well ran its completion twice, the second time on a freed context
    if (!fController || !buffer)
        return kIOReturnBadArgument;

    bool write = (buffer->getDirection() & kIODirectionOut) != 0;
    IOReturn prep = buffer->prepare();
    if (prep != kIOReturnSuccess)
        return prep;

    IOReturn ret = fController->readWrite(write, block, nblks, buffer, 0);
    buffer->complete();
    IOStorage::complete(completion, ret,
        ret == kIOReturnSuccess ? nblks * fController->blockSize() : 0);
    return kIOReturnSuccess;
}

IOReturn
IONVMEDisk::doSynchronize(UInt64 block,
                          UInt64 nblks,
                          IOStorageSynchronizeOptions options)
{
    (void)block;
    (void)nblks;
    (void)options;
    return fController ? fController->flush() : kIOReturnNoDevice;
}

IOReturn IONVMEDisk::doEjectMedia(void) { return kIOReturnUnsupported; }
IOReturn IONVMEDisk::doFormatMedia(UInt64 byteCapacity)
{
    (void)byteCapacity;
    return kIOReturnUnsupported;
}

UInt32
IONVMEDisk::doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const
{
    if (capacities && capacitiesMaxCount && fController)
        capacities[0] = fController->blockCount() * fController->blockSize();
    return 1;
}

char *IONVMEDisk::getVendorString(void) { return fVendor; }
char *IONVMEDisk::getProductString(void) { return fProduct; }
char *IONVMEDisk::getRevisionString(void) { return fRevision; }
char *IONVMEDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn IONVMEDisk::reportBlockSize(UInt64 *blockSize)
{
    if (!blockSize || !fController) return kIOReturnBadArgument;
    *blockSize = fController->blockSize();
    return kIOReturnSuccess;
}

IOReturn IONVMEDisk::reportEjectability(bool *isEjectable)
{
    if (isEjectable) *isEjectable = false;
    return kIOReturnSuccess;
}

IOReturn IONVMEDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
    if (!maxBlock || !fController) return kIOReturnBadArgument;
    UInt64 blocks = fController->blockCount();
    *maxBlock = blocks ? blocks - 1 : 0;
    return kIOReturnSuccess;
}

IOReturn IONVMEDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
    if (mediaPresent) *mediaPresent = fController != NULL;
    if (changedState) *changedState = false;
    return kIOReturnSuccess;
}

IOReturn IONVMEDisk::reportRemovability(bool *isRemovable)
{
    if (isRemovable) *isRemovable = false;
    return kIOReturnSuccess;
}

IOReturn IONVMEDisk::reportWriteProtection(bool *isWriteProtected)
{
    if (isWriteProtected) *isWriteProtected = false;
    return kIOReturnSuccess;
}
