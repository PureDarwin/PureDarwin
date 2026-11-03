#include "PDRockchipDWMMCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(PDRockchipDWMMCDisk, IOBlockStorageDevice);

bool
PDRockchipDWMMCDisk::initWithController(PDRockchipDWMMC *controller)
{
	if (controller == NULL || !init(NULL)) {
		return false;
	}
	fController = controller;
	setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
	return true;
}

IOReturn
PDRockchipDWMMCDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *attributes, IOStorageCompletion *completion)
{
	bool write;
	IOReturn ret;

	(void)attributes;
	if (buffer == NULL) {
		IOStorage::complete(completion, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
	}
	write = (buffer->getDirection() & kIODirectionOut) != 0;
	ret = buffer->prepare();
	if (ret == kIOReturnSuccess) {
		ret = fController->readWrite(write, block, nblks, buffer);
		buffer->complete();
	}
	IOStorage::complete(completion, ret, ret == kIOReturnSuccess ? nblks * 512 : 0);
	return kIOReturnSuccess;
}

// Every transfer finishes on the card before it completes
IOReturn
PDRockchipDWMMCDisk::doSynchronize(UInt64, UInt64, IOStorageSynchronizeOptions)
{
	return kIOReturnSuccess;
}

IOReturn PDRockchipDWMMCDisk::doEjectMedia(void) { return kIOReturnUnsupported; }
IOReturn PDRockchipDWMMCDisk::doFormatMedia(UInt64) { return kIOReturnUnsupported; }

UInt32
PDRockchipDWMMCDisk::doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount != 0) {
		capacities[0] = fController->blockCount() * 512;
	}
	return 1;
}

char *PDRockchipDWMMCDisk::getVendorString(void) { return (char *)"SD"; }
char *PDRockchipDWMMCDisk::getProductString(void) { return (char *)"SD Card"; }
char *PDRockchipDWMMCDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDRockchipDWMMCDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
PDRockchipDWMMCDisk::reportBlockSize(UInt64 *blockSize)
{
	*blockSize = 512;
	return kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMCDisk::reportEjectability(bool *isEjectable)
{
	*isEjectable = false;
	return kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMCDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	UInt64 n = fController->blockCount();

	*maxBlock = n != 0 ? n - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMCDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
	*mediaPresent = true;
	if (changedState != NULL) {
		*changedState = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMCDisk::reportRemovability(bool *isRemovable)
{
	*isRemovable = false;
	return kIOReturnSuccess;
}

IOReturn
PDRockchipDWMMCDisk::reportWriteProtection(bool *isWriteProtected)
{
	*isWriteProtected = fController->isReadOnly();
	return kIOReturnSuccess;
}
