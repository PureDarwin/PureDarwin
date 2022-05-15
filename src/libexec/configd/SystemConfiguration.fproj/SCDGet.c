/*
 * Copyright (c) 2000-2005, 2009-2011, 2013, 2016-2020 Apple Inc. All rights reserved.
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
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */


CFDictionaryRef
SCDynamicStoreCopyMultiple(SCDynamicStoreRef	store,
			   CFArrayRef		keys,
			   CFArrayRef		patterns)
{
	SCDynamicStorePrivateRef	storePrivate;
	kern_return_t			status;
	CFDataRef			xmlKeys		= NULL;	/* keys (XML serialized) */
	xmlData_t			myKeysRef	= NULL;	/* keys (serialized) */
	CFIndex				myKeysLen	= 0;
	CFDataRef			xmlPatterns	= NULL;	/* patterns (XML serialized) */
	xmlData_t			myPatternsRef	= NULL;	/* patterns (serialized) */
	CFIndex				myPatternsLen	= 0;
	xmlDataOut_t			xmlDictRef	= NULL;	/* dict (serialized) */
	mach_msg_type_number_t		xmlDictLen	= 0;
	CFDictionaryRef			dict		= NULL;	/* dict (un-serialized) */
	CFDictionaryRef			expDict		= NULL;	/* dict (un-serialized / expanded) */
	int				sc_status;

	if (!__SCDynamicStoreNormalize(&store, TRUE)) {
		return NULL;
	}

	/* serialize the keys */
	if (keys != NULL) {
		if (!_SCSerialize(keys, &xmlKeys, &myKeysRef, &myKeysLen)) {
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	/* serialize the patterns */
	if (patterns != NULL) {
		if (!_SCSerialize(patterns, &xmlPatterns, &myPatternsRef, &myPatternsLen)) {
			if (xmlKeys != NULL) CFRelease(xmlKeys);
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}
	}

	storePrivate = (SCDynamicStorePrivateRef)store;

    retry :

	/* send the keys and patterns, fetch the associated result from the server */
	status = configget_m(storePrivate->server,
			     myKeysRef,
			     (mach_msg_type_number_t)myKeysLen,
			     myPatternsRef,
			     (mach_msg_type_number_t)myPatternsLen,
			     &xmlDictRef,
			     &xmlDictLen,
			     (int *)&sc_status);

	if (__SCDynamicStoreCheckRetryAndHandleError(store,
						     status,
						     &sc_status,
						     "SCDynamicStoreCopyMultiple configget_m()")) {
		goto retry;
	}

	if (sc_status != kSCStatusOK) {
		if (xmlDictRef != NULL) {
			(void) vm_deallocate(mach_task_self(), (vm_address_t)xmlDictRef, xmlDictLen);
		}
		_SCErrorSet(sc_status);
		goto done;
	}

	/* un-serialize the dictionary */
	if (!_SCUnserialize((CFPropertyListRef *)&dict, NULL, xmlDictRef, xmlDictLen)) {
		_SCErrorSet(kSCStatusFailed);
		goto done;
	}

	expDict = _SCUnserializeMultiple(dict);
	CFRelease(dict);

    done:

	/* clean up */
	if (xmlKeys != NULL)		CFRelease(xmlKeys);
	if (xmlPatterns != NULL)	CFRelease(xmlPatterns);

	return expDict;
}


CFPropertyListRef
SCDynamicStoreCopyValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate;
	kern_return_t			status;
	CFDataRef			utfKey;			/* key (XML serialized) */
	xmlData_t			myKeyRef;		/* key (serialized) */
	CFIndex				myKeyLen;
	xmlDataOut_t			xmlDataRef	= NULL;	/* data (serialized) */
	mach_msg_type_number_t		xmlDataLen	= 0;
	CFPropertyListRef		data;			/* data (un-serialized) */
	int				newInstance;
	int				sc_status;

	if (!__SCDynamicStoreNormalize(&store, TRUE)) {
		return NULL;
	}

	storePrivate = (SCDynamicStorePrivateRef)store;

	if (storePrivate->cache_active) {
		if ((storePrivate->cached_set != NULL) &&
		    CFDictionaryGetValueIfPresent(storePrivate->cached_set, key, (const void **)&data)) {
			// if we have "set" a new value
			return (CFRetain(data));
		}

		if ((storePrivate->cached_removals != NULL) &&
		    CFArrayContainsValue(storePrivate->cached_removals,
					 CFRangeMake(0, CFArrayGetCount(storePrivate->cached_removals)),
					 key)) {
			// if we have "removed" the key
			_SCErrorSet(kSCStatusNoKey);
			return NULL;
		}

		if ((storePrivate->cached_keys != NULL) &&
		    CFDictionaryGetValueIfPresent(storePrivate->cached_keys, key, (const void **)&data)) {
			// if we have a cached value
			return (CFRetain(data));
		}
	}

	/* serialize the key */
	if (!_SCSerializeString(key, &utfKey, &myKeyRef, &myKeyLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

    retry :

	/* send the key & fetch the associated data from the server */
	status = configget(storePrivate->server,
			   myKeyRef,
			   (mach_msg_type_number_t)myKeyLen,
			   &xmlDataRef,
			   &xmlDataLen,
			   &newInstance,
			   (int *)&sc_status);

	if (__SCDynamicStoreCheckRetryAndHandleError(store,
						     status,
						     &sc_status,
						     "SCDynamicStoreCopyValue configget()")) {
		goto retry;
	}

	/* clean up */
	CFRelease(utfKey);

	if (sc_status != kSCStatusOK) {
		if (xmlDataRef != NULL) {
			(void) vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
		}
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the data */
	if (!_SCUnserialize(&data, NULL, xmlDataRef, xmlDataLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	if (storePrivate->cache_active && (data != NULL)) {
		if (storePrivate->cached_keys == NULL) {
			storePrivate->cached_keys = CFDictionaryCreateMutable(NULL,
									      0,
									      &kCFTypeDictionaryKeyCallBacks,
									      &kCFTypeDictionaryValueCallBacks);
		}
		CFDictionarySetValue(storePrivate->cached_keys, key, data);
	}

	return data;
}
