#ifndef _PUREDARWIN_PDBCM2835SDDISK_H
#define _PUREDARWIN_PDBCM2835SDDISK_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include "PDBcm2835SD.h"

class PDBcm2835SDDisk : public IOBlockStorageDevice
{
	OSDeclareDefaultStructors(PDBcm2835SDDisk);

public:
	bool initWithController(PDBcm2835SD *controller);
	void free(void) APPLE_KEXT_OVERRIDE;

	IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
	    UInt64 nblks, IOStorageAttributes *attributes,
	    IOStorageCompletion *completion) APPLE_KEXT_OVERRIDE;
	IOReturn doSynchronize(UInt64 block, UInt64 nblks,
	    IOStorageSynchronizeOptions options = 0) APPLE_KEXT_OVERRIDE;
	IOReturn doEjectMedia(void) APPLE_KEXT_OVERRIDE;
	IOReturn doFormatMedia(UInt64 byteCapacity) APPLE_KEXT_OVERRIDE;
	UInt32   doGetFormatCapacities(UInt64 *capacities,
	    UInt32 capacitiesMaxCount) const APPLE_KEXT_OVERRIDE;

	char *getVendorString(void) APPLE_KEXT_OVERRIDE;
	char *getProductString(void) APPLE_KEXT_OVERRIDE;
	char *getRevisionString(void) APPLE_KEXT_OVERRIDE;
	char *getAdditionalDeviceInfoString(void) APPLE_KEXT_OVERRIDE;

	IOReturn reportBlockSize(UInt64 *blockSize) APPLE_KEXT_OVERRIDE;
	IOReturn reportEjectability(bool *isEjectable) APPLE_KEXT_OVERRIDE;
	IOReturn reportMaxValidBlock(UInt64 *maxBlock) APPLE_KEXT_OVERRIDE;
	IOReturn reportMediaState(bool *mediaPresent,
	    bool *changedState = 0) APPLE_KEXT_OVERRIDE;
	IOReturn reportRemovability(bool *isRemovable) APPLE_KEXT_OVERRIDE;
	IOReturn reportWriteProtection(bool *isWriteProtected) APPLE_KEXT_OVERRIDE;

private:
	PDBcm2835SD *fController;
};

#endif /* _PUREDARWIN_PDBCM2835SDDISK_H */
