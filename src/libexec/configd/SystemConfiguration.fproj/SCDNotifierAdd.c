/*
 * Copyright (c) 2000-2005, 2009-2011, 2013, 2016, 2017, 2019, 2020 Apple Inc. All rights reserved.
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


static void
addKey(CFMutableArrayRef *keysP, CFStringRef key)
{
	if (*keysP == NULL) {
		*keysP = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	CFArrayAppendValue(*keysP, key);
	return;
}


Boolean
SCDynamicStoreAddWatchedKey(SCDynamicStoreRef store, CFStringRef key, Boolean isRegex)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			utfKey;		/* serialized key */
	xmlData_t			myKeyRef;
	CFIndex				myKeyLen;
	int				sc_status;

	if (!__SCDynamicStoreNormalize(&store, FALSE)) {
		return FALSE;
	}

	/* serialize the key */
	if (!_SCSerializeString(key, &utfKey, &myKeyRef, &myKeyLen)) {
		_SCErrorSet(kSCStatusFailed);
		return FALSE;
	}

    retry :

	/* send the key to the server */
	status = notifyadd(storePrivate->server,
			   myKeyRef,
			   (mach_msg_type_number_t)myKeyLen,
			   isRegex,
			   (int *)&sc_status);

	if (__SCDynamicStoreCheckRetryAndHandleError(store,
						     status,
						     &sc_status,
						     "SCDynamicStoreAddWatchedKey notifyadd()")) {
		goto retry;
	}

	/* clean up */
	CFRelease(utfKey);

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	if (isRegex) {
		addKey(&storePrivate->patterns, key);
	} else {
		addKey(&storePrivate->keys, key);
	}
	return TRUE;
}
