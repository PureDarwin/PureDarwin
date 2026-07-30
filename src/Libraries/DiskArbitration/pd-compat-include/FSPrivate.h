/*
 * Apple's libFS (the FSCore/CoreServices volume-format library) is not open
 * source. diskarbitrationd calls into it from exactly three places, all of them
 * descriptive rather than functional:
 *
 *   _FSCopyNameForVolumeFormatAtNode / ...AtURL
 *       the human-readable format name for a volume ("Mac OS Extended",
 *       "MS-DOS (FAT32)"). NULL here, so a disk's description carries no
 *       volume-kind string. Probing, mounting, and which filesystem is actually
 *       chosen are unaffected - those come from the .fs bundle's own probe.
 *
 *   _FSGetMediaEncryptionStatusAtPath
 *       whether the media is encrypted. Reported as not encrypted, which is
 *       accurate: PureDarwin has no FileVault and no CoreStorage.
 *
 *   FSCompareVolumeRole
 *       compares APFS volume roles. Reports "not equal" for everything, the
 *       honest answer with no APFS volume roles to compare.
 */

#ifndef _PUREDARWIN_FSPRIVATE_H_
#define _PUREDARWIN_FSPRIVATE_H_

#include <CoreFoundation/CoreFoundation.h>

CF_INLINE CFStringRef
_FSCopyNameForVolumeFormatAtNode(CFStringRef node)
{
	(void)node;
	return NULL;
}

CF_INLINE CFStringRef
_FSCopyNameForVolumeFormatAtURL(CFURLRef url)
{
	(void)url;
	return NULL;
}

CF_INLINE int
_FSGetMediaEncryptionStatusAtPath(const char *path, Boolean *encrypted,
				  CFDictionaryRef *detail)
{
	(void)path;
	if (encrypted != NULL) {
		*encrypted = FALSE;
	}
	if (detail != NULL) {
		*detail = NULL;
	}
	return 0;
}

CF_INLINE Boolean
FSCompareVolumeRole(CFStringRef role1, CFStringRef role2)
{
	(void)role1;
	(void)role2;
	return FALSE;
}

#endif /* _PUREDARWIN_FSPRIVATE_H_ */
