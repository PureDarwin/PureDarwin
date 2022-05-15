/*
 * Copyright(c) 2000-2021 Apple Inc. All rights reserved.
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
 * February 16, 2004		Allan Nathanson <ajn@apple.com>
 * - add preference notification APIs
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <TargetConditionals.h>
#include <fcntl.h>
#include <pthread.h>
#include <sandbox.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/cdefs.h>
#include <dispatch/dispatch.h>

#include "SCPreferencesInternal.h"
#include "SCD.h"
#include "SCHelper_client.h"


const AuthorizationRef	kSCPreferencesUseEntitlementAuthorization	= (AuthorizationRef)CFSTR("UseEntitlement");


__private_extern__ os_log_t
__log_SCPreferences(void)
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "SCPreferences");
	}

	return log;
}


static CFStringRef
__SCPreferencesCopyDescription(CFTypeRef cf) {
	CFAllocatorRef		allocator	= CFGetAllocator(cf);
	SCPreferencesPrivateRef prefsPrivate	= (SCPreferencesPrivateRef)cf;
	CFMutableStringRef	result;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCPreferences %p [%p]> {"), cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("name = %@"), prefsPrivate->name);
	CFStringAppendFormat(result, NULL, CFSTR(", id = %@"),
			     prefsPrivate->prefsID != NULL ? prefsPrivate->prefsID : CFSTR("[default]"));
	CFStringAppendFormat(result, NULL, CFSTR(", path = %s"),
			     prefsPrivate->path);
	if (prefsPrivate->accessed) {
		CFStringAppendFormat(result, NULL, CFSTR(", accessed"));
	}
	if (prefsPrivate->changed) {
		CFStringAppendFormat(result, NULL, CFSTR(", changed"));
	}
	if (prefsPrivate->locked) {
		CFStringAppendFormat(result, NULL, CFSTR(", locked"));
	}
	if (prefsPrivate->helper_port != MACH_PORT_NULL) {
		CFStringAppendFormat(result, NULL, CFSTR(", helper port = 0x%x"), prefsPrivate->helper_port);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCPreferencesDeallocate(CFTypeRef cf)
{
	SCPreferencesRef	prefs		= (SCPreferencesRef)cf;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	SC_log(LOG_DEBUG, "release %@", prefsPrivate);

	if (prefsPrivate->locked) {
		__SCPreferencesUpdateLockedState(prefs, FALSE);
	}

	/* release resources */

	pthread_mutex_destroy(&prefsPrivate->lock);

	if (prefsPrivate->parent != NULL) {
		SCPreferencesPrivateRef	parentPrivate	= (SCPreferencesPrivateRef)prefsPrivate->parent;

		// remove [weak] reference from parent to this companion
		pthread_mutex_lock(&parentPrivate->lock);
		CFDictionaryRemoveValue(parentPrivate->companions, prefsPrivate->prefsID);
		pthread_mutex_unlock(&parentPrivate->lock);

		// remove [strong] reference from companion to parent
		CFRelease(prefsPrivate->parent);
	}
	if (prefsPrivate->companions != NULL)	CFRelease(prefsPrivate->companions);

	if (prefsPrivate->name)			CFRelease(prefsPrivate->name);
	if (prefsPrivate->prefsID)		CFRelease(prefsPrivate->prefsID);
	if (prefsPrivate->options)		CFRelease(prefsPrivate->options);
	if (prefsPrivate->path)			CFAllocatorDeallocate(NULL, prefsPrivate->path);
	if (prefsPrivate->lockFD != -1)	{
		if (prefsPrivate->lockPath != NULL) {
			unlink(prefsPrivate->lockPath);
		}
		close(prefsPrivate->lockFD);
	}
	if (prefsPrivate->lockPath)		CFAllocatorDeallocate(NULL, prefsPrivate->lockPath);
	if (prefsPrivate->signature)		CFRelease(prefsPrivate->signature);
	if (prefsPrivate->sessionNoO_EXLOCK != NULL) {
		CFRelease(prefsPrivate->sessionNoO_EXLOCK);
	}
	if (prefsPrivate->sessionKeyLock)	CFRelease(prefsPrivate->sessionKeyLock);
	if (prefsPrivate->sessionKeyCommit)	CFRelease(prefsPrivate->sessionKeyCommit);
	if (prefsPrivate->sessionKeyApply)	CFRelease(prefsPrivate->sessionKeyApply);
	if (prefsPrivate->rlsContext.release != NULL) {
		(*prefsPrivate->rlsContext.release)(prefsPrivate->rlsContext.info);
	}
	if (prefsPrivate->prefs)		CFRelease(prefsPrivate->prefs);
	if (prefsPrivate->authorizationData != NULL) CFRelease(prefsPrivate->authorizationData);
	if (prefsPrivate->helper_port != MACH_PORT_NULL) {
		(void) _SCHelperExec(prefsPrivate->helper_port,
				     SCHELPER_MSG_PREFS_CLOSE,
				     NULL,
				     NULL,
				     NULL);
		_SCHelperClose(&prefsPrivate->helper_port);
	}

	return;
}


static CFTypeID __kSCPreferencesTypeID	= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCPreferencesClass = {
	0,				// version
	"SCPreferences",		// className
	NULL,				// init
	NULL,				// copy
	__SCPreferencesDeallocate,	// dealloc
	NULL,				// equal
	NULL,				// hash
	NULL,				// copyFormattingDesc
	__SCPreferencesCopyDescription	// copyDebugDesc
};


static pthread_once_t initialized	= PTHREAD_ONCE_INIT;

static void
__SCPreferencesInitialize(void) {
	/* register with CoreFoundation */
	__kSCPreferencesTypeID = _CFRuntimeRegisterClass(&__SCPreferencesClass);
	return;
}


static SCPreferencesPrivateRef
__SCPreferencesCreatePrivate(CFAllocatorRef	allocator)
{
	SCPreferencesPrivateRef	prefsPrivate;
	uint32_t		size;

	/* initialize runtime */
	pthread_once(&initialized, __SCPreferencesInitialize);

	/* allocate prefs session */
	size  = sizeof(SCPreferencesPrivate) - sizeof(CFRuntimeBase);
	prefsPrivate = (SCPreferencesPrivateRef)_CFRuntimeCreateInstance(allocator,
									 __kSCPreferencesTypeID,
									 size,
									 NULL);
	if (prefsPrivate == NULL) {
		return NULL;
	}

	/* initialize non-zero/NULL members */
	pthread_mutex_init(&prefsPrivate->lock, NULL);
	prefsPrivate->lockFD				= -1;
	prefsPrivate->isRoot				= (geteuid() == 0);

	return prefsPrivate;
}


__private_extern__ Boolean
__SCPreferencesCreate_helper(SCPreferencesRef prefs)
{
	CFDataRef		data		= NULL;
	CFMutableDictionaryRef	info;
	CFNumberRef		num;
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;
	CFStringRef		str;
	uint32_t		pid		= getpid();

	// start helper
	ok = _SCHelperOpen(prefsPrivate->authorizationData,
			   &prefsPrivate->helper_port);
	if (!ok) {
		goto fail;
	}

	// create a dictionary of information to pass to the helper
	info = CFDictionaryCreateMutable(NULL,
					 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

	// save prefsID
	if (prefsPrivate->prefsID != NULL) {
		CFDictionarySetValue(info, CFSTR("prefsID"), prefsPrivate->prefsID);
	}

	// save options
	if (prefsPrivate->options != NULL) {
		CFDictionarySetValue(info, CFSTR("options"), prefsPrivate->options);
	}

	// save preferences session "name"
	CFDictionarySetValue(info, CFSTR("name"), prefsPrivate->name);

	// save PID
	num = CFNumberCreate(NULL, kCFNumberSInt32Type, &pid);
	CFDictionarySetValue(info, CFSTR("PID"), num);
	CFRelease(num);

	// save process name
	str = CFStringCreateWithCString(NULL, getprogname(), kCFStringEncodingUTF8);
	CFDictionarySetValue(info, CFSTR("PROC_NAME"), str);
	CFRelease(str);

	// serialize the info
	ok = _SCSerialize(info, &data, NULL, NULL);
	CFRelease(info);
	if (data == NULL || !ok) {
		goto fail;
	}

	// have the helper "open" the prefs
	ok = _SCHelperExec(prefsPrivate->helper_port,
			   SCHELPER_MSG_PREFS_OPEN,
			   data,
			   &status,
			   NULL);
	if (data != NULL) CFRelease(data);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

	return TRUE;

    fail :

	// close helper
	if (prefsPrivate->helper_port != MACH_PORT_NULL) {
		_SCHelperClose(&prefsPrivate->helper_port);
	}

	status = kSCStatusAccessError;

    error :

	// return error
	_SCErrorSet(status);
	return FALSE;
}


static Boolean
__SCPreferencesAccess_helper(SCPreferencesRef prefs)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFDictionaryRef		serverDict	= NULL;
	CFDictionaryRef		serverPrefs	= NULL;
	CFDictionaryRef		serverSignature	= NULL;
	uint32_t		status		= kSCStatusOK;
	CFDataRef		reply		= NULL;

	if (prefsPrivate->helper_port == MACH_PORT_NULL) {
		ok = __SCPreferencesCreate_helper(prefs);
		if (!ok) {
			return FALSE;
		}
	}

	// have the helper "access" the prefs
	ok = _SCHelperExec(prefsPrivate->helper_port,
			   SCHELPER_MSG_PREFS_ACCESS,
			   NULL,
			   &status,
			   &reply);
	if (!ok) {
		goto fail;
	}

	if (status != kSCStatusOK) {
		goto error;
	}

	if (reply == NULL) {
		goto fail;
	}

	ok = _SCUnserialize((CFPropertyListRef *)&serverDict, reply, NULL, 0);
	CFRelease(reply);
	if (!ok) {
		goto fail;
	}

	if (isA_CFDictionary(serverDict)) {
		serverPrefs = CFDictionaryGetValue(serverDict, CFSTR("preferences"));
		serverPrefs = isA_CFDictionary(serverPrefs);

		serverSignature = CFDictionaryGetValue(serverDict, CFSTR("signature"));
		serverSignature = isA_CFData(serverSignature);
	}

	if ((serverPrefs == NULL) || (serverSignature == NULL)) {
		if (serverDict != NULL) CFRelease(serverDict);
		goto fail;
	}

	prefsPrivate->prefs     = CFDictionaryCreateMutableCopy(NULL, 0, serverPrefs);
	prefsPrivate->signature = CFRetain(serverSignature);
	prefsPrivate->accessed  = TRUE;
	CFRelease(serverDict);

	return TRUE;

    fail :

	// close helper
	if (prefsPrivate->helper_port != MACH_PORT_NULL) {
		_SCHelperClose(&prefsPrivate->helper_port);
	}

	status = kSCStatusAccessError;

    error :

	// return error
	_SCErrorSet(status);
	return FALSE;
}


static SCPreferencesPrivateRef
__SCPreferencesCreate(CFAllocatorRef	allocator,
		      CFStringRef	name,
		      CFStringRef	prefsID,
		      CFDataRef		authorizationData,
		      CFDictionaryRef	options)
{
	SCPreferencesPrivateRef		prefsPrivate;
	int				sc_status	= kSCStatusOK;

	/*
	 * allocate and initialize a new prefs session
	 */
	prefsPrivate = __SCPreferencesCreatePrivate(allocator);
	if (prefsPrivate == NULL) {
		return NULL;
	}

	prefsPrivate->name = CFStringCreateCopy(allocator, name);
	if (prefsID != NULL) {
		prefsPrivate->prefsID = CFStringCreateCopy(allocator, prefsID);
	}
	if (authorizationData != NULL) {
		prefsPrivate->authorizationData = CFRetain(authorizationData);
	}
	if (options != NULL) {
		prefsPrivate->options = CFDictionaryCreateCopy(allocator, options);
	}

	/*
	 * convert prefsID to path
	 */
	prefsPrivate->path = __SCPreferencesPath(allocator, prefsID);
	if (prefsPrivate->path == NULL) {
		sc_status = kSCStatusFailed;
		goto error;
	}

	if (access(prefsPrivate->path, R_OK) == 0) {
		goto done;
	}

	switch (errno) {
		case ENOENT :
			/* no preference data, start fresh */
			sc_status = kSCStatusNoConfigFile;
			goto done;
		case EPERM  :
		case EACCES :
			if (prefsPrivate->authorizationData != NULL) {
				/* no problem, we'll be using the helper */
				goto done;
			}

			SC_log(LOG_NOTICE, "open() failed: %s", strerror(errno));
			sc_status = kSCStatusAccessError;
			break;
		default :
			SC_log(LOG_NOTICE, "open() failed: %s", strerror(errno));
			sc_status = kSCStatusFailed;
			break;
	}

    error:

	CFRelease(prefsPrivate);
	_SCErrorSet(sc_status);
	return NULL;

    done :

	/* all OK */
	_SCErrorSet(sc_status);
	return prefsPrivate;
}


static void
processHardwareDependency(SCPreferencesRef prefs)
{
	CFStringRef 		new_model;
	CFStringRef		old_model;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFBooleanRef		val		= NULL;

	if (!__SCPreferencesUsingDefaultPrefs(prefs)) {
		// if not default [/L/P/SC/preferences.plist] preferences
		return;
	}

	if ((prefsPrivate->options != NULL) &&
	    CFDictionaryGetValueIfPresent(prefsPrivate->options,
					  kSCPreferencesOptionAllowModelConflict,
					  (const void **)&val) &&
	    isA_CFBoolean(val) &&
	    CFBooleanGetValue(val)) {
		// if we are preserving configurations with model conflicts
		return;
	}

	// check if we need to regenerate the configuration for a new model
	old_model = CFDictionaryGetValue(prefsPrivate->prefs, MODEL);
	new_model = _SC_hw_model(FALSE);
	if ((old_model != NULL) && !_SC_CFEqual(old_model, new_model)) {
		SC_log(LOG_NOTICE, "Hardware model changed\n"
				   "  created on \"%@\"\n"
				   "  now on     \"%@\"",
		       old_model,
		       new_model);

		CFDictionaryRemoveAllValues(prefsPrivate->prefs);
		prefsPrivate->changed = TRUE;
	}

	return;
}


__private_extern__ void
__SCPreferencesAccess(SCPreferencesRef	prefs)
{
	CFAllocatorRef		allocator	= CFGetAllocator(prefs);
	int			fd		= -1;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	struct stat		statBuf;

	if (prefsPrivate->accessed) {
		// if preference data has already been accessed
		return;
	}

	if (access(prefsPrivate->path, R_OK) == 0) {
		fd = open(prefsPrivate->path, O_RDONLY, 0644);
	} else {
		fd = -1;
	}
	if (fd != -1) {
		// create signature
		if (fstat(fd, &statBuf) == -1) {
			SC_log(LOG_NOTICE, "fstat() failed: %s", strerror(errno));
			memset(&statBuf, 0, sizeof(statBuf));
		}
	} else {
		switch (errno) {
			case ENOENT :
				/* no preference data, start fresh */
				break;
			case EPERM  :
			case EACCES :
				if (prefsPrivate->authorizationData != NULL) {
					if (__SCPreferencesAccess_helper(prefs)) {
						goto done;
					} else {
						SC_log(LOG_NOTICE, "__SCPreferencesAccess_helper() failed: %s",
						       SCErrorString(SCError()));
					}
					break;
				}
				// fall through
			default :
				SC_log(LOG_NOTICE, "open() failed: %s", strerror(errno));
				break;
		}
		memset(&statBuf, 0, sizeof(statBuf));
	}

	if (prefsPrivate->signature != NULL) CFRelease(prefsPrivate->signature);
	prefsPrivate->signature = __SCPSignatureFromStatbuf(&statBuf);

	if (statBuf.st_size > 0) {
		CFDictionaryRef		dict;
		CFErrorRef		error	= NULL;
		CFMutableDataRef	xmlData;

		/*
		 * extract property list
		 */
		xmlData = CFDataCreateMutable(allocator, (CFIndex)statBuf.st_size);
		CFDataSetLength(xmlData, (CFIndex)statBuf.st_size);
		if (read(fd, (void *)CFDataGetBytePtr(xmlData), (CFIndex)statBuf.st_size) != (CFIndex)statBuf.st_size) {
			/* corrupt prefs file, start fresh */
			SC_log(LOG_INFO, "read(): could not load preference data");
			CFRelease(xmlData);
			xmlData = NULL;
			goto done;
		}

		/*
		 * load preferences
		 */
		dict = CFPropertyListCreateWithData(allocator, xmlData, kCFPropertyListImmutable, NULL, &error);
		CFRelease(xmlData);
		if (dict == NULL) {
			/* corrupt prefs file, start fresh */
			if (error != NULL) {
				SC_log(LOG_NOTICE, "CFPropertyListCreateWithData(): %@", error);
				CFRelease(error);
			}
			goto done;
		}

		/*
		 * make sure that we've got a dictionary
		 */
		if (!isA_CFDictionary(dict)) {
			/* corrupt prefs file, start fresh */
			SC_log(LOG_INFO, "CFGetTypeID(): not a dictionary");
			CFRelease(dict);
			goto done;
		}

		prefsPrivate->prefs = CFDictionaryCreateMutableCopy(allocator, 0, dict);
		CFRelease(dict);

		/*
		 * check hardware dependency
		 */
		processHardwareDependency(prefs);
	}

    done :

	if (fd != -1) {
		(void) close(fd);
	}

	if (prefsPrivate->prefs == NULL) {
		/*
		 * new file, create empty preferences
		 */
//		SC_log(LOG_INFO, "creating new preferences file");
		prefsPrivate->prefs = CFDictionaryCreateMutable(allocator,
								0,
								&kCFTypeDictionaryKeyCallBacks,
								&kCFTypeDictionaryValueCallBacks);
		prefsPrivate->changed = FALSE;
	}

	SC_log(LOG_DEBUG, "SCPreferences() access: %s, size=%lld",
	       prefsPrivate->path,
	       __SCPreferencesPrefsSize(prefs));

	prefsPrivate->accessed = TRUE;
	return;
}


SCPreferencesRef
SCPreferencesCreate(CFAllocatorRef		allocator,
		    CFStringRef			name,
		    CFStringRef			prefsID)
{
	SCPreferencesPrivateRef	prefsPrivate;

	prefsPrivate = __SCPreferencesCreate(allocator, name, prefsID, NULL, NULL);
	if (prefsPrivate != NULL) {
		SC_log(LOG_DEBUG, "create %@", prefsPrivate);
	}

	return (SCPreferencesRef)prefsPrivate;
}


SCPreferencesRef
SCPreferencesCreateWithAuthorization(CFAllocatorRef	allocator,
				     CFStringRef	name,
				     CFStringRef	prefsID,
				     AuthorizationRef	authorization)
{
	SCPreferencesRef	prefs;

#if	!TARGET_OS_IPHONE
	if (authorization == NULL) {
		authorization = kSCPreferencesUseEntitlementAuthorization;
	}
#else	// !TARGET_OS_IPHONE
	authorization = kSCPreferencesUseEntitlementAuthorization;
#endif	// !TARGET_OS_IPHONE

	prefs = SCPreferencesCreateWithOptions(allocator, name, prefsID, authorization, NULL);
	return prefs;
}


SCPreferencesRef
SCPreferencesCreateWithOptions(CFAllocatorRef	allocator,
			       CFStringRef	name,
			       CFStringRef	prefsID,
			       AuthorizationRef	authorization,
			       CFDictionaryRef	options)
{
	CFDataRef			authorizationData	= NULL;
	SCPreferencesPrivateRef		prefsPrivate;

	if (options != NULL) {
		if (!isA_CFDictionary(options)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	}

	if (authorization != NULL) {
		CFMutableDictionaryRef	authorizationDict;
		CFStringRef		bundleID;

		authorizationDict =  CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
#if	!TARGET_OS_IPHONE
		if (authorization != kSCPreferencesUseEntitlementAuthorization) {
			CFDataRef			data;
			AuthorizationExternalForm	extForm;
			OSStatus			os_status;

			os_status = AuthorizationMakeExternalForm(authorization, &extForm);
			if (os_status != errAuthorizationSuccess) {
				SC_log(LOG_INFO, "AuthorizationMakeExternalForm() failed");
				_SCErrorSet(kSCStatusInvalidArgument);
				CFRelease(authorizationDict);
				return NULL;
			}

			data = CFDataCreate(NULL, (const UInt8 *)extForm.bytes, sizeof(extForm.bytes));
			CFDictionaryAddValue(authorizationDict,
					     kSCHelperAuthAuthorization,
					     data);
			CFRelease(data);
		}
#endif	// !TARGET_OS_IPHONE

		bundleID = _SC_getApplicationBundleID();
		CFDictionaryAddValue(authorizationDict,
				     kSCHelperAuthCallerInfo,
				     bundleID);

		if (authorizationDict != NULL) {
			(void) _SCSerialize((CFPropertyListRef)authorizationDict,
					    &authorizationData,
					    NULL,
					    NULL);
			CFRelease(authorizationDict);
		}
	}

	prefsPrivate = __SCPreferencesCreate(allocator, name, prefsID, authorizationData, options);
	if (prefsPrivate != NULL) {
		const char	*opt_none	= "";
		const char	*opt_1		= opt_none;
		const char	*opt_2		= opt_none;

		if (options != NULL) {
			opt_2 = "options";
		}

		if (authorization != NULL) {
			if (authorization == kSCPreferencesUseEntitlementAuthorization) {
				opt_1 = "entitlement";
			} else {
				opt_1 = "authorization";
			}
		}

		SC_log(LOG_DEBUG, "create w/%s%s%s %@",
		       opt_2,
		       ((opt_2 != opt_none) && (opt_1 != opt_none)) ? " + " : "",
		       opt_1,
		       prefsPrivate);
	}

	if (authorizationData != NULL) CFRelease(authorizationData);

	return (SCPreferencesRef)prefsPrivate;
}


SCPreferencesRef
SCPreferencesCreateCompanion(SCPreferencesRef prefs, CFStringRef companionPrefsID)
{
	CFAllocatorRef		allocator	= CFGetAllocator(prefs);
	SCPreferencesPrivateRef	companionPrefs	= NULL;
	CFMutableStringRef	newPrefsID;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (companionPrefsID == NULL) {
		companionPrefsID = PREFS_DEFAULT_CONFIG;
	} else {
		if (CFStringFindWithOptions(companionPrefsID,
					    CFSTR("/"),
					    CFRangeMake(0, CFStringGetLength(companionPrefsID)),
					    kCFCompareBackwards,
					    NULL)) {
			// if companion prefsID contains a "/"
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	}

	if (prefsPrivate->prefsID == NULL) {
		if (CFEqual(companionPrefsID, PREFS_DEFAULT_CONFIG)) {
			// if prefsID and companionPrefsID match
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
		newPrefsID = CFStringCreateMutableCopy(allocator, 0, companionPrefsID);
	} else {
		CFIndex	prefsIDLen	= CFStringGetLength(prefsPrivate->prefsID);
		CFRange	range;

		if (CFStringFindWithOptions(prefsPrivate->prefsID,
					    CFSTR("/"),
					    CFRangeMake(0, prefsIDLen),
					    kCFCompareBackwards,
					    &range)) {
			Boolean		match;
			CFStringRef	suffix;

			// if slash, check suffix
			range.location++;
			if (range.location >= prefsIDLen) {
				// if no suffix
				_SCErrorSet(kSCStatusInvalidArgument);
				return NULL;
			}
			range.length = prefsIDLen - range.location;
			suffix = CFStringCreateWithSubstring(allocator, prefsPrivate->prefsID, range);
			match = CFEqual(suffix, companionPrefsID);
			CFRelease(suffix);
			if (match) {
				// if prefsID [suffix] and companionPrefsID match
				_SCErrorSet(kSCStatusInvalidArgument);
				return NULL;
			}

			// replace the suffix
			newPrefsID = CFStringCreateMutableCopy(NULL, 0, prefsPrivate->prefsID);
			CFStringReplace(newPrefsID, range, companionPrefsID);
		} else if (!CFEqual(prefsPrivate->prefsID, companionPrefsID)) {
			// if no slash, prefsID and companionPrefsID differ
			newPrefsID = CFStringCreateMutableCopy(NULL, 0, companionPrefsID);
		} else {
			// if no slash, prefsID and companionPrefsID match
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	}
	assert(newPrefsID != NULL);

	pthread_mutex_lock(&prefsPrivate->lock);
	if ((prefsPrivate->companions != NULL) &&
	    CFDictionaryGetValueIfPresent(prefsPrivate->companions,
					  newPrefsID,
					  (const void **)&companionPrefs) &&
	    (companionPrefs != NULL)) {
		// if we already have a companion
		SC_log(LOG_DEBUG, "create [companion] reference %@", companionPrefs);
		CFRetain(companionPrefs);
	} else {
		companionPrefs = __SCPreferencesCreate(allocator,
						       prefsPrivate->name,
						       newPrefsID,
						       prefsPrivate->authorizationData,
						       prefsPrivate->options);
		if (companionPrefs != NULL) {
			SCPreferencesPrivateRef	companionPrefsPrivate	= (SCPreferencesPrivateRef)companionPrefs;

			SC_log(LOG_DEBUG, "create [companion] %@", companionPrefs);

			// add [strong] reference from companion to parent
			companionPrefsPrivate->parent = CFRetain(prefs);

			// add [weak] reference from parent to this companion
			if (prefsPrivate->companions == NULL) {
				prefsPrivate->companions = CFDictionaryCreateMutable(NULL,
										     0,
										     &kCFTypeDictionaryKeyCallBacks,
										     NULL);
			}
			CFDictionarySetValue(prefsPrivate->companions, newPrefsID, companionPrefs);
		}
	}
	pthread_mutex_unlock(&prefsPrivate->lock);

	CFRelease(newPrefsID);

	return (SCPreferencesRef)companionPrefs;
}


CFTypeID
SCPreferencesGetTypeID(void) {
	pthread_once(&initialized, __SCPreferencesInitialize);	/* initialize runtime */
	return __kSCPreferencesTypeID;
}


static void
prefsNotify(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
#pragma unused(store)
	void				*context_info;
	void				(*context_release)(const void *);
	CFIndex				i;
	CFIndex				n;
	SCPreferencesNotification       notify		= 0;
	SCPreferencesRef		prefs		= (SCPreferencesRef)info;
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	SCPreferencesCallBack		rlsFunction;

	n = (changedKeys != NULL) ? CFArrayGetCount(changedKeys) : 0;
	for (i = 0; i < n; i++) {
		CFStringRef     key;

		key = CFArrayGetValueAtIndex(changedKeys, i);

		// check if "commit"
		if (CFEqual(key, prefsPrivate->sessionKeyCommit)) {
			// if preferences have been saved
			notify |= kSCPreferencesNotificationCommit;
			continue;
		}

		// check if "apply"
		if (CFEqual(key, prefsPrivate->sessionKeyApply)) {
			// if stored preferences should be applied to current configuration
			notify |= kSCPreferencesNotificationApply;
			continue;
		}
	}

	if (notify == 0) {
		// if no changes
		return;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	/* callout */
	rlsFunction = prefsPrivate->rlsFunction;
	if (prefsPrivate->rlsContext.retain != NULL) {
		context_info	= (void *)prefsPrivate->rlsContext.retain(prefsPrivate->rlsContext.info);
		context_release	= prefsPrivate->rlsContext.release;
	} else {
		context_info	= prefsPrivate->rlsContext.info;
		context_release	= NULL;
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	if (rlsFunction != NULL) {
		SC_log(LOG_DEBUG, "exec SCPreferences callout: %s%s%s",
		       ((notify & kSCPreferencesNotificationCommit) != 0) ? "commit" : "",
		       (((notify & kSCPreferencesNotificationCommit) != 0) &&
			((notify & kSCPreferencesNotificationApply ) != 0)) ? ", " : "",
		       ((notify & kSCPreferencesNotificationApply)  != 0) ? "apply"  : "");
		(*rlsFunction)(prefs, notify, context_info);
	}

	if (context_release != NULL) {
		(*context_release)(context_info);
	}

	return;
}


__private_extern__ void
__SCPreferencesAddSessionKeys(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	/* create the session "commit" key */
	if (prefsPrivate->sessionKeyCommit == NULL) {
		prefsPrivate->sessionKeyCommit = _SCPNotificationKey(NULL,
								     prefsPrivate->prefsID,
								     kSCPreferencesKeyCommit);
	}

	/* create the session "apply" key */
	if (prefsPrivate->sessionKeyApply == NULL) {
		prefsPrivate->sessionKeyApply = _SCPNotificationKey(NULL,
								    prefsPrivate->prefsID,
								    kSCPreferencesKeyApply);
	}

	return;
}


__private_extern__ Boolean
__SCPreferencesAddSession(SCPreferencesRef prefs)
{
	CFAllocatorRef			allocator	= CFGetAllocator(prefs);
	SCDynamicStoreContext		context		= { 0
							  , (void *)prefs
							  , CFRetain
							  , CFRelease
							  , CFCopyDescription
							  };
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefsPrivate->sessionRefcnt == 0) {
		/* establish a dynamic store session */
		prefsPrivate->session = SCDynamicStoreCreate(allocator,
							     prefsPrivate->name,
							     prefsNotify,
							     &context);
		if (prefsPrivate->session == NULL) {
			SC_log(LOG_ERR, "could not add SCDynamicStore session (for prefs): %s",
			       SCErrorString(SCError()));
			return FALSE;
		}

		SC_log(LOG_DEBUG, "added SCDynamicStore session (for prefs)");
	}

	prefsPrivate->sessionRefcnt++;
	return TRUE;
}


__private_extern__ void
__SCPreferencesRemoveSession(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (prefsPrivate->sessionRefcnt > 0) {
		if (--prefsPrivate->sessionRefcnt == 0) {
			CFRelease(prefsPrivate->session);
			prefsPrivate->session = NULL;

			SC_log(LOG_DEBUG, "removed SCDynamicStore session (for prefs)");
		}
	}

	return;
}


static void
appendLockedPreferences(const void *key, const void *value, void *context)
{
#pragma unused(key)
	CFMutableStringRef	str	= (CFMutableStringRef)context;

	CFStringAppendFormat(str, NULL, CFSTR("%s%@"),
			     (CFStringGetLength(str) > 0) ? "\n" : "",
			     value);
	return;
}


__private_extern__ void
__SCPreferencesUpdateLockedState(SCPreferencesRef prefs, Boolean locked)
{
	static dispatch_queue_t		lockedQueue;
	static CFMutableDictionaryRef	lockedState;
	static dispatch_once_t		once;
	SCPreferencesPrivateRef		prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	dispatch_once(&once, ^{
		os_state_block_t	state_block;

		lockedQueue = dispatch_queue_create("SCPreferences locked state queue", NULL);

		lockedState = CFDictionaryCreateMutable(NULL,
							0,
							NULL,	// NO retain/release
							&kCFTypeDictionaryValueCallBacks);

		state_block = ^os_state_data_t(os_state_hints_t hints) {
#pragma unused(hints)
			CFDataRef		data	= NULL;
			Boolean			ok;
			os_state_data_t		state_data;
			size_t			state_data_size;
			CFIndex			state_len;
			CFMutableStringRef	str;

			if (CFDictionaryGetCount(lockedState) == 0) {
				// if no locked preferences
				return NULL;
			}

			str = CFStringCreateMutable(NULL, 0);
			CFDictionaryApplyFunction(lockedState, appendLockedPreferences, str);
			ok = _SCSerialize(str, &data, NULL, NULL);
			CFRelease(str);

			state_len = (ok && (data != NULL)) ? CFDataGetLength(data) : 0;
			state_data_size = OS_STATE_DATA_SIZE_NEEDED(state_len);
			if (state_data_size > MAX_STATEDUMP_SIZE) {
				SC_log(LOG_ERR, "locked SCPreferences : state data too large (%zd > %zd)",
				       state_data_size,
				       (size_t)MAX_STATEDUMP_SIZE);
				if (data != NULL) CFRelease(data);
				return NULL;
			}

			state_data = calloc(1, state_data_size);
			if (state_data == NULL) {
				SC_log(LOG_ERR, "locked SCPreferences: could not allocate state data");
				if (data != NULL) CFRelease(data);
				return NULL;
			}

			state_data->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
			state_data->osd_data_size = (uint32_t)state_len;
			strlcpy(state_data->osd_title, "open/locked SCPreferences", sizeof(state_data->osd_title));
			if (state_len > 0) {
				memcpy(state_data->osd_data, CFDataGetBytePtr(data), state_len);
			}
			if (data != NULL) CFRelease(data);

			return state_data;
		};

		(void) os_state_add_handler(lockedQueue, state_block);
	});

	// update the locked state
	prefsPrivate->locked = locked;

	// add (or update) the locked preferences
	dispatch_sync(lockedQueue, ^{
		if (locked) {
			CFStringRef	str;

			str = CFCopyDescription(prefs);
			CFDictionarySetValue(lockedState, prefs, str);
			CFRelease(str);
		} else {
			CFDictionaryRemoveValue(lockedState, prefs);
		}
	});

	return;
}


Boolean
SCPreferencesSetCallback(SCPreferencesRef       prefs,
			 SCPreferencesCallBack  callout,
			 SCPreferencesContext   *context)
{
	SCPreferencesPrivateRef	prefsPrivate = (SCPreferencesPrivateRef)prefs;

	if (!isA_SCPreferences(prefs)) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	pthread_mutex_lock(&prefsPrivate->lock);

	if (prefsPrivate->rlsContext.release != NULL) {
		/* let go of the current context */
		(*prefsPrivate->rlsContext.release)(prefsPrivate->rlsContext.info);
	}

	prefsPrivate->rlsFunction 			= callout;
	prefsPrivate->rlsContext.info			= NULL;
	prefsPrivate->rlsContext.retain			= NULL;
	prefsPrivate->rlsContext.release		= NULL;
	prefsPrivate->rlsContext.copyDescription	= NULL;
	if (context != NULL) {
		memcpy(&prefsPrivate->rlsContext, context, sizeof(SCPreferencesContext));
		if (context->retain != NULL) {
			prefsPrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	pthread_mutex_unlock(&prefsPrivate->lock);

	return TRUE;
}


static Boolean
__SCPreferencesScheduleWithRunLoop(SCPreferencesRef	prefs,
				   CFRunLoopRef		runLoop,
				   CFStringRef		runLoopMode,
				   dispatch_queue_t	queue)
{
	Boolean			ok		= FALSE;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	pthread_mutex_lock(&prefsPrivate->lock);

	if ((prefsPrivate->dispatchQueue != NULL) ||		// if we are already scheduled on a dispatch queue
	    ((queue != NULL) && prefsPrivate->scheduled)) {	// if we are already scheduled on a CFRunLoop
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	if (!prefsPrivate->scheduled) {
		CFMutableArrayRef       keys;

		// add SCDynamicStore session (for notifications) ... and hold a 'prefs' reference
		if (prefsPrivate->session == NULL) {
			ok = __SCPreferencesAddSession(prefs);
			if (!ok) {
				goto done;
			}
			assert(prefsPrivate->session != NULL);
		}

		// add SCDynamicStore "keys"
		__SCPreferencesAddSessionKeys(prefs);

		keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(keys, prefsPrivate->sessionKeyCommit);
		CFArrayAppendValue(keys, prefsPrivate->sessionKeyApply);
		ok = SCDynamicStoreSetNotificationKeys(prefsPrivate->session, keys, NULL);
		CFRelease(keys);
		if (!ok) {
			SC_log(LOG_ERR, "could not set SCDynamicStore notification keys (for prefs): %s",
			       SCErrorString(SCError()));
			goto done;
		}

		if (runLoop != NULL) {
			prefsPrivate->rls = SCDynamicStoreCreateRunLoopSource(NULL, prefsPrivate->session, 0);
			if (prefsPrivate->rls == NULL) {
				SC_log(LOG_ERR, "could not create SCDynamicStore runloop source (for prefs): %s",
				       SCErrorString(SCError()));
				ok = FALSE;
				goto done;
			}
			prefsPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}

		SC_log(LOG_DEBUG, "scheduled");

		prefsPrivate->scheduled = TRUE;
	}

	if (queue != NULL) {
		ok = SCDynamicStoreSetDispatchQueue(prefsPrivate->session, queue);
		if (!ok) {
			prefsPrivate->scheduled = FALSE;
			(void) SCDynamicStoreSetNotificationKeys(prefsPrivate->session, NULL, NULL);
			__SCPreferencesRemoveSession(prefs);
			goto done;
		}

		prefsPrivate->dispatchQueue = queue;
		dispatch_retain(prefsPrivate->dispatchQueue);
	} else {
		if (!_SC_isScheduled(NULL, runLoop, runLoopMode, prefsPrivate->rlList)) {
			/*
			 * if we do not already have notifications scheduled with
			 * this runLoop / runLoopMode
			 */
			CFRunLoopAddSource(runLoop, prefsPrivate->rls, runLoopMode);
		}

		_SC_schedule(prefs, runLoop, runLoopMode, prefsPrivate->rlList);
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);
	return ok;
}


static Boolean
__SCPreferencesUnscheduleFromRunLoop(SCPreferencesRef	prefs,
				     CFRunLoopRef	runLoop,
				     CFStringRef	runLoopMode)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	CFIndex			n		= 0;
	Boolean			ok		= FALSE;

	pthread_mutex_lock(&prefsPrivate->lock);

	if ((runLoop != NULL) && !prefsPrivate->scheduled) {			// if we should be scheduled (but are not)
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	if (((runLoop == NULL) && (prefsPrivate->dispatchQueue == NULL)) ||	// if we should be scheduled on a dispatch queue (but are not)
	    ((runLoop != NULL) && (prefsPrivate->dispatchQueue != NULL))) {	// if we should be scheduled on a CFRunLoop (but are scheduled on a dispatch queue)
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	if (runLoop == NULL) {
		SCDynamicStoreSetDispatchQueue(prefsPrivate->session, NULL);
		dispatch_release(prefsPrivate->dispatchQueue);
		prefsPrivate->dispatchQueue = NULL;
	} else {
		if (!_SC_unschedule(prefs, runLoop, runLoopMode, prefsPrivate->rlList, FALSE)) {
			// if not currently scheduled on this runLoop / runLoopMode
			_SCErrorSet(kSCStatusInvalidArgument);
			goto done;
		}

		n = CFArrayGetCount(prefsPrivate->rlList);
		if (n == 0 || !_SC_isScheduled(NULL, runLoop, runLoopMode, prefsPrivate->rlList)) {
			/*
			 * if we are no longer scheduled to receive notifications for
			 * this runLoop / runLoopMode
			 */
			CFRunLoopRemoveSource(runLoop, prefsPrivate->rls, runLoopMode);

			if (n == 0) {
				// if *all* notifications have been unscheduled
				CFRelease(prefsPrivate->rlList);
				prefsPrivate->rlList = NULL;
				CFRunLoopSourceInvalidate(prefsPrivate->rls);
				CFRelease(prefsPrivate->rls);
				prefsPrivate->rls = NULL;
			}
		}
	}

	if (n == 0) {
		CFArrayRef      changedKeys;

		SC_log(LOG_DEBUG, "unscheduled");

		// if *all* notifications have been unscheduled
		prefsPrivate->scheduled = FALSE;

		// no need to track changes
		(void) SCDynamicStoreSetNotificationKeys(prefsPrivate->session, NULL, NULL);

		// clear out any pending notifications
		changedKeys = SCDynamicStoreCopyNotifiedKeys(prefsPrivate->session);
		if (changedKeys != NULL) {
			CFRelease(changedKeys);
		}

		// remove SCDynamicStore session, release 'prefs' reference
		__SCPreferencesRemoveSession(prefs);
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&prefsPrivate->lock);
	return ok;
}


Boolean
SCPreferencesScheduleWithRunLoop(SCPreferencesRef       prefs,
				 CFRunLoopRef		runLoop,
				 CFStringRef		runLoopMode)
{
	if (!isA_SCPreferences(prefs) || (runLoop == NULL) || (runLoopMode == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	return __SCPreferencesScheduleWithRunLoop(prefs, runLoop, runLoopMode, NULL);
}


Boolean
SCPreferencesUnscheduleFromRunLoop(SCPreferencesRef     prefs,
				   CFRunLoopRef		runLoop,
				   CFStringRef		runLoopMode)
{
	if (!isA_SCPreferences(prefs) || (runLoop == NULL) || (runLoopMode == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	return __SCPreferencesUnscheduleFromRunLoop(prefs, runLoop, runLoopMode);
}


Boolean
SCPreferencesSetDispatchQueue(SCPreferencesRef	prefs,
			      dispatch_queue_t	queue)
{
	Boolean	ok	= FALSE;

	if (!isA_SCPreferences(prefs)) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return FALSE;
	}

	if (queue != NULL) {
		ok = __SCPreferencesScheduleWithRunLoop(prefs, NULL, NULL, queue);
	} else {
		ok = __SCPreferencesUnscheduleFromRunLoop(prefs, NULL, NULL);
	}

	return ok;
}


static void
__SCPreferencesSynchronize_helper(SCPreferencesRef prefs)
{
	Boolean			ok;
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;
	uint32_t		status		= kSCStatusOK;

	if (prefsPrivate->helper_port == MACH_PORT_NULL) {
		// if no helper
		return;
	}

	// have the helper "synchronize" the prefs
	ok = _SCHelperExec(prefsPrivate->helper_port,
			   SCHELPER_MSG_PREFS_SYNCHRONIZE,
			   NULL,
			   &status,
			   NULL);
	if (!ok) {
		// close helper
		if (prefsPrivate->helper_port != MACH_PORT_NULL) {
			_SCHelperClose(&prefsPrivate->helper_port);
		}
	}

	return;
}


void
SCPreferencesSynchronize(SCPreferencesRef prefs)
{
	SCPreferencesPrivateRef	prefsPrivate	= (SCPreferencesPrivateRef)prefs;

	if (!isA_SCPreferences(prefs)) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoPrefsSession);
		return;
	}

	SC_log(LOG_DEBUG, "SCPreferences() synchronize: %s",
	       prefsPrivate->path);

	if (prefsPrivate->authorizationData != NULL) {
		__SCPreferencesSynchronize_helper(prefs);
	}
	if (prefsPrivate->prefs != NULL) {
		CFRelease(prefsPrivate->prefs);
		prefsPrivate->prefs = NULL;
	}
	if (prefsPrivate->signature != NULL) {
		CFRelease(prefsPrivate->signature);
		prefsPrivate->signature = NULL;
	}
	prefsPrivate->accessed = FALSE;
	prefsPrivate->changed  = FALSE;

	return;
}
