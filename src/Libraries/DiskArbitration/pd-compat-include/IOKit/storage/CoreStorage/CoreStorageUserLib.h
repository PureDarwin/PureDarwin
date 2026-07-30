/*
 * CoreStorage is Apple's logical volume manager (the layer under FileVault 2).
 * It is not open source, and PureDarwin has no CoreStorage kext.
 */

#ifndef _PUREDARWIN_CORESTORAGEUSERLIB_H_
#define _PUREDARWIN_CORESTORAGEUSERLIB_H_

typedef void *CoreStorageLogicalRef;
typedef void *CoreStorageFamilyRef;

#define kCoreStorageLogicalFamilyUUIDKey "PureDarwinUnverifiedCoreStorageFamilyUUIDKey"

#endif /* _PUREDARWIN_CORESTORAGEUSERLIB_H_ */
