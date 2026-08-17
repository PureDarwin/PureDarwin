#include "PDBcm2835SDDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(PDBcm2835SDDisk, IOBlockStorageDevice);

bool
PDBcm2835SDDisk::initWithController(PDBcm2835SD *controller)
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
PDBcm2835SDDisk::free(void)
{
	fController = NULL;
	super::free();
}

IOReturn
PDBcm2835SDDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
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
PDBcm2835SDDisk::doSynchronize(UInt64 block, UInt64 nblks,
    IOStorageSynchronizeOptions options)
{
	(void)block;
	(void)nblks;
	(void)options;
	/* Every transfer is completed synchronously against the card. */
	return kIOReturnSuccess;
}

IOReturn PDBcm2835SDDisk::doEjectMedia(void) { return kIOReturnUnsupported; }

IOReturn
PDBcm2835SDDisk::doFormatMedia(UInt64 byteCapacity)
{
	(void)byteCapacity;
	return kIOReturnUnsupported;
}

UInt32
PDBcm2835SDDisk::doGetFormatCapacities(UInt64 *capacities,
    UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount != 0 && fController != NULL) {
		capacities[0] = fController->blockCount() * fController->blockSize();
	}
	return 1;
}

char *PDBcm2835SDDisk::getVendorString(void)   { return (char *)"Broadcom"; }
char *PDBcm2835SDDisk::getProductString(void)  { return (char *)"SD Card"; }
char *PDBcm2835SDDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDBcm2835SDDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
PDBcm2835SDDisk::reportBlockSize(UInt64 *blockSize)
{
	if (blockSize == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	*blockSize = fController->blockSize();
	return kIOReturnSuccess;
}

IOReturn
PDBcm2835SDDisk::reportEjectability(bool *isEjectable)
{
	if (isEjectable != NULL) {
		*isEjectable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDBcm2835SDDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	if (maxBlock == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	UInt64 blocks = fController->blockCount();
	*maxBlock = blocks ? blocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
PDBcm2835SDDisk::reportMediaState(bool *mediaPresent, bool *changedState)
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
PDBcm2835SDDisk::reportRemovability(bool *isRemovable)
{
	if (isRemovable != NULL) {
		*isRemovable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDBcm2835SDDisk::reportWriteProtection(bool *isWriteProtected)
{
	if (isWriteProtected != NULL) {
		*isWriteProtected = fController != NULL && fController->isReadOnly();
	}
	return kIOReturnSuccess;
}
