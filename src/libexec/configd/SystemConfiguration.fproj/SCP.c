/*
 * Copyright (c) 2000, 2001, 2003-2005, 2007-2009, 2011, 2014-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "SCPreferencesInternal.h"
#include "SCNetworkConfigurationInternal.h"

#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/param.h>

__private_extern__ CF_RETURNS_RETAINED CFDataRef
__SCPSignatureFromStatbuf(const struct stat *statBuf)
{
	CFMutableDataRef	signature;
	SCPSignatureDataRef	sig;

	signature = CFDataCreateMutable(NULL, sizeof(SCPSignatureData));
	CFDataSetLength(signature, sizeof(SCPSignatureData));

	/* ALIGN: CFDataGetBytePtr aligns to at least 8 bytes */
	sig = (SCPSignatureDataRef)(void *)CFDataGetBytePtr(signature);

	sig->st_dev       = statBuf->st_dev;
	sig->st_ino       = statBuf->st_ino;
	sig->tv_sec       = statBuf->st_mtimespec.tv_sec;
	sig->tv_nsec      = statBuf->st_mtimespec.tv_nsec;
	sig->st_size      = statBuf->st_size;
	return signature;
}


__private_extern__
uint32_t
__SCPreferencesGetNetworkConfigurationFlags(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	return (prefs != NULL) ? prefsPrivate->nc_flags : 0;
}

__private_extern__
void
__SCPreferencesSetNetworkConfigurationFlags(SCPreferencesRef prefs, uint32_t nc_flags)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefs != NULL) {
		prefsPrivate->nc_flags = nc_flags;
	}

	return;
}

#if	TARGET_OS_OSX
#include <os/boot_mode_private.h>
#include <limits.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>

static const char *
get_boot_mode(void)
{
	const char *	mode = NULL;
	bool		success;

	success = os_boot_mode_query(&mode);
	if (!success) {
		SC_log(LOG_NOTICE, "os_boot_mode_query failed");
		return (NULL);
	}
	return (mode);
}

static const char *
get_preboot_path(void)
{
	static dispatch_once_t	once;
	static char *		path;

#define PREBOOT_SYSCTL		"kern.apfsprebootuuid"
#define PREBOOT_PATH_FORMAT	"/System/Volumes/Preboot/%s"

	dispatch_once(&once, ^{
		uuid_string_t	preboot_uuid;
		int 		result;
		size_t		size;

		size = sizeof(preboot_uuid);
		result = sysctlbyname(PREBOOT_SYSCTL,
				      preboot_uuid, &size, NULL, 0);
		if (result != 0) {
			SC_log(LOG_NOTICE, "sysctlbyname(%s) failed %s (%d)",
			       PREBOOT_SYSCTL, strerror(errno), errno);
		}
		else {
			char	temp[PATH_MAX];

			snprintf(temp, sizeof(temp),
				 PREBOOT_PATH_FORMAT, preboot_uuid);
			path = strdup(temp);
			SC_log(LOG_NOTICE, "Preboot path '%s'", path);
		}
	});
	return (path);
}

/*
 * get_preboot_path_prefix
 * - when in the FVUnlock Preboot environment, retrieve the path to the
 *   Preboot folder, which contains these two SCPreferences-managed files:
 *	Library/Preferences/SystemConfiguration/preferences.plist
 *	Library/Preferences/SystemConfiguration/NetworkInterfaces.plist
 */
static const char *
get_preboot_path_prefix(void)
{
	const char *	mode;
	const char *	path = NULL;

	mode = get_boot_mode();
	if (mode == NULL) {
		goto done;
	}
	if (strcmp(mode, OS_BOOT_MODE_FVUNLOCK) != 0) {
		goto done;
	}
	path = get_preboot_path();
 done:
	return (path);
}

#else	// TARGET_OS_OSX

static const char *
get_preboot_path_prefix(void)
{
	return (NULL);
}

#endif 	// TARGET_OS_OSX

__private_extern__ char *
__SCPreferencesPath(CFAllocatorRef	allocator,
		    CFStringRef		prefsID)
{
	CFStringRef	path		= NULL;
	char		*pathStr;

	if (prefsID == NULL) {
		const char *	prefix;
		const char *	preboot_path;

		preboot_path = get_preboot_path_prefix();
		prefix = (preboot_path != NULL)? preboot_path : "";
		/* default preference ID */
		path = CFStringCreateWithFormat(allocator,
						NULL,
						CFSTR("%s%@/%@"),
						prefix,
						PREFS_DEFAULT_DIR,
						PREFS_DEFAULT_CONFIG);
		if (preboot_path != NULL) {
			SC_log(LOG_DEBUG,
			       "SCPreferences using path '%@'",
			       path);
		}
	} else if (CFStringHasPrefix(prefsID, CFSTR("/"))) {
		/* if absolute path */
		path = CFStringCreateCopy(allocator, prefsID);
	} else {
		/* prefsID using default directory */
		const char *	prefix;
		const char *	preboot_path;

		if (CFEqual(prefsID, PREFS_DEFAULT_CONFIG) ||
		    CFEqual(prefsID, INTERFACES_DEFAULT_CONFIG)) {
			preboot_path = get_preboot_path_prefix();
		} else {
			preboot_path = NULL;
		}
		prefix = (preboot_path != NULL)? preboot_path : "";
		path = CFStringCreateWithFormat(allocator,
						NULL,
						CFSTR("%s%@/%@"),
						prefix,
						PREFS_DEFAULT_DIR,
						prefsID);
		if (CFStringHasSuffix(prefsID, CFSTR(".xml"))) {
			CFMutableStringRef	newPath;

			newPath = CFStringCreateMutableCopy(allocator, 0, path);
			CFStringReplace(newPath,
					CFRangeMake(CFStringGetLength(newPath)-4, 4),
					CFSTR(".plist"));
			CFRelease(path);
			path = newPath;
		}
		if (preboot_path != NULL) {
			SC_log(LOG_DEBUG,
			       "SCPreferences using path '%@'",
			       path);
		}
	}

	/*
	 * convert CFStringRef path to C-string path
	 */
	pathStr = _SC_cfstring_to_cstring(path, NULL, 0, kCFStringEncodingASCII);
	if (pathStr == NULL) {
		CFIndex pathLen;

		pathLen = CFStringGetMaximumSizeOfFileSystemRepresentation(path);
		pathStr = CFAllocatorAllocate(NULL, pathLen, 0);
		if (!CFStringGetFileSystemRepresentation(path, pathStr, pathLen)) {
			SC_log(LOG_INFO, "could not convert path to C string");
			CFAllocatorDeallocate(NULL, pathStr);
			pathStr = NULL;
		}
	}

	CFRelease(path);
	return pathStr;
}


__private_extern__
Boolean
__SCPreferencesIsEmpty(SCPreferencesRef	prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	assert(prefs != NULL);
	__SCPreferencesAccess(prefs);

	if ((prefsPrivate->prefs == NULL) ||
	    (CFDictionaryGetCount(prefsPrivate->prefs) == 0)) {
		return TRUE;
	}

	return FALSE;
}


__private_extern__
off_t
__SCPreferencesPrefsSize(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	SCPSignatureDataRef	sig;
	CFDataRef		signature;

	signature = prefsPrivate->signature;
	if (signature == NULL) {
		return 0;
	}

	sig = (SCPSignatureDataRef)(void *)CFDataGetBytePtr(signature);
	return sig->st_size;
}


__private_extern__
Boolean
__SCPreferencesUsingDefaultPrefs(SCPreferencesRef prefs)
{
	char			*curPath;
	Boolean			isDefault = FALSE;
	SCPreferencesPrivateRef prefsPrivate = (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		// if no prefs, assume that we are using the "default" prefs
		return TRUE;
	}

	curPath = prefsPrivate->path;
	if (curPath != NULL) {
		char*	defPath;

		defPath = __SCPreferencesPath(NULL, NULL);
		if (defPath != NULL) {
			if (strcmp(curPath, defPath) == 0) {
				isDefault = TRUE;
			}
			CFAllocatorDeallocate(NULL, defPath);
		}
	}
	return isDefault;
}

CFDataRef
SCPreferencesGetSignature(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefs == NULL) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return NULL;
	}

	__SCPreferencesAccess(prefs);

	return prefsPrivate->signature;
}


__private_extern__ CF_RETURNS_RETAINED CFStringRef
_SCPNotificationKey(CFAllocatorRef	allocator,
		    CFStringRef		prefsID,
		    int			keyType)
{
	CFStringRef	keyStr;
	char		*path;
	CFStringRef	pathStr;
	CFStringRef	storeKey;

	switch (keyType) {
		case kSCPreferencesKeyLock :
			keyStr = CFSTR("lock");
			break;
		case kSCPreferencesKeyCommit :
			keyStr = CFSTR("commit");
			break;
		case kSCPreferencesKeyApply :
			keyStr = CFSTR("apply");
			break;
		default :
			return NULL;
	}

	path = __SCPreferencesPath(allocator, prefsID);
	if (path == NULL) {
		return NULL;
	}

	pathStr = CFStringCreateWithCStringNoCopy(allocator,
						  path,
						  kCFStringEncodingUTF8,
						  kCFAllocatorNull);

	storeKey = CFStringCreateWithFormat(allocator,
					    NULL,
					    CFSTR("%@%@:%@"),
					    kSCDynamicStoreDomainPrefs,
					    keyStr,
					    pathStr);

	CFRelease(pathStr);
	CFAllocatorDeallocate(NULL, path);
	return storeKey;
}


CFStringRef
SCDynamicStoreKeyCreatePreferences(CFAllocatorRef	allocator,
				   CFStringRef		prefsID,
				   SCPreferencesKeyType	keyType)
{
	return _SCPNotificationKey(allocator, prefsID, keyType);
}

