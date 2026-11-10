// SPDX-License-Identifier: MIT
// block storage nub for the tegra210 emmc

#include "PDTegraSDMMCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(PDTegraSDMMCDisk, IOBlockStorageDevice);

bool
PDTegraSDMMCDisk::initWithController(PDTegraSDMMC *controller)
{
	if (controller == NULL) {
		return false;
	}
	if (!init(NULL)) {
		return false;
	}

	// opendirectoryd walks the storage parents for this, soldered emmc is internal secure digital
	OSDictionary *protocol = OSDictionary::withCapacity(2);
	OSString *type = OSString::withCString(kIOPropertyPhysicalInterconnectTypeSecureDigital);
	OSString *location = OSString::withCString(kIOPropertyInternalKey);
	bool configured = protocol != NULL && type != NULL && location != NULL &&
	    protocol->setObject(kIOPropertyPhysicalInterconnectTypeKey, type) &&
	    protocol->setObject(kIOPropertyPhysicalInterconnectLocationKey, location) &&
	    setProperty(kIOPropertyProtocolCharacteristicsKey, protocol);
	OSSafeReleaseNULL(type);
	OSSafeReleaseNULL(location);
	OSSafeReleaseNULL(protocol);
	if (!configured) return false;

	fController = controller;
	setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
	setName(controller->isSD() ? "sd0" : "emmc0");
	setLocation("0");
	return true;
}

void
PDTegraSDMMCDisk::free(void)
{
	fController = NULL;
	super::free();
}

IOReturn
PDTegraSDMMCDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *attributes,
    IOStorageCompletion *completion)
{
	(void)attributes;

	// an error return means not started, and the caller completes the request itself: completing it
	// here too ran IOBlockStorageDriver's completion twice, on a freed context
	if (fController == NULL || buffer == NULL)
		return kIOReturnBadArgument;

	bool write = (buffer->getDirection() & kIODirectionOut) != 0;
	IOReturn ret = buffer->prepare();

	if (ret == kIOReturnSuccess) {
		ret = write ? fController->write(block, nblks, buffer) : fController->read(block, nblks, buffer);
		buffer->complete();
	}

	IOStorage::complete(completion, ret,
	    ret == kIOReturnSuccess ? nblks * fController->blockSize() : 0);
	return kIOReturnSuccess;
}

IOReturn
PDTegraSDMMCDisk::doSynchronize(UInt64 block, UInt64 nblks,
    IOStorageSynchronizeOptions options)
{
	(void)block;
	(void)nblks;
	(void)options;
	return kIOReturnSuccess;
}

IOReturn PDTegraSDMMCDisk::doEjectMedia(void) { return kIOReturnUnsupported; }

IOReturn
PDTegraSDMMCDisk::doFormatMedia(UInt64 byteCapacity)
{
	(void)byteCapacity;
	return kIOReturnUnsupported;
}

UInt32
PDTegraSDMMCDisk::doGetFormatCapacities(UInt64 *capacities,
    UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount != 0 && fController != NULL) {
		capacities[0] = fController->blockCount() * fController->blockSize();
	}
	return 1;
}

char *PDTegraSDMMCDisk::getVendorString(void)   { return (char *)"NVIDIA"; }
char *
PDTegraSDMMCDisk::getProductString(void)
{
	return fController != NULL ? (char *)fController->product() : (char *)"eMMC";
}

char *PDTegraSDMMCDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDTegraSDMMCDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
PDTegraSDMMCDisk::reportBlockSize(UInt64 *blockSize)
{
	if (blockSize == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	*blockSize = fController->blockSize();
	return kIOReturnSuccess;
}

IOReturn
PDTegraSDMMCDisk::reportEjectability(bool *isEjectable)
{
	if (isEjectable != NULL) {
		*isEjectable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDTegraSDMMCDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	if (maxBlock == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	UInt64 blocks = fController->blockCount();
	*maxBlock = blocks ? blocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
PDTegraSDMMCDisk::reportMediaState(bool *mediaPresent, bool *changedState)
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
PDTegraSDMMCDisk::reportRemovability(bool *isRemovable)
{
	if (isRemovable != NULL) {
		*isRemovable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDTegraSDMMCDisk::reportWriteProtection(bool *isWriteProtected)
{
	if (isWriteProtected != NULL) {
		*isWriteProtected = fController == NULL || !fController->writable();
	}
	return kIOReturnSuccess;
}
