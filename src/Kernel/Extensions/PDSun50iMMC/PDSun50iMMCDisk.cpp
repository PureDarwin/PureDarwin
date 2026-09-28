#include "PDSun50iMMCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(PDSun50iMMCDisk, IOBlockStorageDevice);

bool
PDSun50iMMCDisk::initWithController(PDSun50iMMC *controller)
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
PDSun50iMMCDisk::free(void)
{
	fController = NULL;
	super::free();
}

IOReturn
PDSun50iMMCDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *attributes,
    IOStorageCompletion *completion)
{
	(void)attributes;

	if (fController == NULL || buffer == NULL) {
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
PDSun50iMMCDisk::doSynchronize(UInt64 block, UInt64 nblks,
    IOStorageSynchronizeOptions options)
{
	(void)block;
	(void)nblks;
	(void)options;
	// every transfer completes synchronously against the card
	return kIOReturnSuccess;
}

IOReturn PDSun50iMMCDisk::doEjectMedia(void) { return kIOReturnUnsupported; }

IOReturn
PDSun50iMMCDisk::doFormatMedia(UInt64 byteCapacity)
{
	(void)byteCapacity;
	return kIOReturnUnsupported;
}

UInt32
PDSun50iMMCDisk::doGetFormatCapacities(UInt64 *capacities,
    UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount != 0 && fController != NULL) {
		capacities[0] = fController->blockCount() * fController->blockSize();
	}
	return 1;
}

char *PDSun50iMMCDisk::getVendorString(void)   { return (char *)"Allwinner"; }
char *PDSun50iMMCDisk::getProductString(void)  { return (char *)"SD Card"; }
char *PDSun50iMMCDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDSun50iMMCDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
PDSun50iMMCDisk::reportBlockSize(UInt64 *blockSize)
{
	if (blockSize == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	*blockSize = fController->blockSize();
	return kIOReturnSuccess;
}

IOReturn
PDSun50iMMCDisk::reportEjectability(bool *isEjectable)
{
	if (isEjectable != NULL) {
		*isEjectable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDSun50iMMCDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	if (maxBlock == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	UInt64 blocks = fController->blockCount();
	*maxBlock = blocks ? blocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
PDSun50iMMCDisk::reportMediaState(bool *mediaPresent, bool *changedState)
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
PDSun50iMMCDisk::reportRemovability(bool *isRemovable)
{
	if (isRemovable != NULL) {
		*isRemovable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDSun50iMMCDisk::reportWriteProtection(bool *isWriteProtected)
{
	if (isWriteProtected != NULL) {
		*isWriteProtected = fController != NULL && fController->isReadOnly();
	}
	return kIOReturnSuccess;
}
