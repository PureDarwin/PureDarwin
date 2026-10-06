#include "PDSg2002SDDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(PDSg2002SDDisk, IOBlockStorageDevice);

bool
PDSg2002SDDisk::initWithController(PDSg2002SD *controller)
{
	if (controller == NULL) {
		return false;
	}
	if (!init(NULL)) {
		return false;
	}

	fController = controller;
	setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
	setName("sd0");
	setLocation("0");
	return true;
}

void
PDSg2002SDDisk::free(void)
{
	fController = NULL;
	super::free();
}

IOReturn
PDSg2002SDDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *attributes,
    IOStorageCompletion *completion)
{
	(void)attributes;

	// an error return means not started and IOBlockStorageDriver completes the request itself:
	// completing here as well ran its completion twice, the second time on a freed context
	if (fController == NULL || buffer == NULL)
		return kIOReturnBadArgument;

	bool write = (buffer->getDirection() & kIODirectionOut) != 0;

	IOReturn prep = buffer->prepare();
	if (prep != kIOReturnSuccess)
		return prep;

	IOReturn ret = fController->readWrite(write, block, nblks, buffer);
	buffer->complete();

	IOStorage::complete(completion, ret,
	    ret == kIOReturnSuccess ? nblks * fController->blockSize() : 0);
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SDDisk::doSynchronize(UInt64 block, UInt64 nblks,
    IOStorageSynchronizeOptions options)
{
	(void)block;
	(void)nblks;
	(void)options;
	// every transfer completes synchronously against the card
	return kIOReturnSuccess;
}

IOReturn PDSg2002SDDisk::doEjectMedia(void) { return kIOReturnUnsupported; }

IOReturn
PDSg2002SDDisk::doFormatMedia(UInt64 byteCapacity)
{
	(void)byteCapacity;
	return kIOReturnUnsupported;
}

UInt32
PDSg2002SDDisk::doGetFormatCapacities(UInt64 *capacities,
    UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount != 0 && fController != NULL) {
		capacities[0] = fController->blockCount() * fController->blockSize();
	}
	return 1;
}

char *PDSg2002SDDisk::getVendorString(void)   { return (char *)"Sophgo"; }
char *PDSg2002SDDisk::getProductString(void)  { return (char *)"SD Card"; }
char *PDSg2002SDDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDSg2002SDDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
PDSg2002SDDisk::reportBlockSize(UInt64 *blockSize)
{
	if (blockSize == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	*blockSize = fController->blockSize();
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SDDisk::reportEjectability(bool *isEjectable)
{
	if (isEjectable != NULL) {
		*isEjectable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SDDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	if (maxBlock == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	UInt64 blocks = fController->blockCount();
	*maxBlock = blocks ? blocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SDDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
	if (mediaPresent != NULL) {
		*mediaPresent = fController != NULL;
	}
	if (changedState != NULL) {
		*changedState = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SDDisk::reportRemovability(bool *isRemovable)
{
	if (isRemovable != NULL) {
		*isRemovable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDSg2002SDDisk::reportWriteProtection(bool *isWriteProtected)
{
	if (isWriteProtected != NULL) {
		*isWriteProtected = fController != NULL && fController->isReadOnly();
	}
	return kIOReturnSuccess;
}
