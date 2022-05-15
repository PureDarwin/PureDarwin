/*
 * Copyright (c) 2000, 2001, 2003, 2004, 2006-2012, 2015, 2016, 2019 Apple Inc. All rights reserved.
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

#include <unistd.h>

#include "configd.h"
#include "session.h"

static Boolean
isMySessionKey(CFStringRef sessionKey, CFStringRef key)
{
	CFDictionaryRef	dict;
	CFStringRef	storeSessionKey;

	dict = CFDictionaryGetValue(storeData, key);
	if (!dict) {
		/* if key no longer exists */
		return FALSE;
	}

	storeSessionKey = CFDictionaryGetValue(dict, kSCDSession);
	if (!storeSessionKey) {
		/* if this is not a session key */
		return FALSE;
	}

	if (!CFEqual(sessionKey, storeSessionKey)) {
		/* if this is not "my" session key */
		return FALSE;
	}

	return TRUE;
}


static void
removeAllKeys(SCDynamicStoreRef store, Boolean isRegex)
{
	SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)store;
	CFArrayRef			keys;
	CFIndex				n;

	keys = isRegex ? storePrivate->patterns : storePrivate->keys;
	n = (keys != NULL) ? CFArrayGetCount(keys) : 0;
	if (n > 0) {
		CFIndex		i;
		CFArrayRef	keysToRemove;

		keysToRemove = CFArrayCreateCopy(NULL, keys);
		for (i = 0; i < n; i++) {
			(void) __SCDynamicStoreRemoveWatchedKey(store,
								CFArrayGetValueAtIndex(keysToRemove, i),
								isRegex,
								TRUE);
		}
		CFRelease(keysToRemove);
	}

	return;
}


__private_extern__
int
__SCDynamicStoreClose(SCDynamicStoreRef *store)
{
	serverSessionRef		mySession;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)*store;

	SC_trace("close   : %5d",
		 storePrivate->server);

	/* Remove all notification keys and patterns */
	removeAllKeys(*store, FALSE);	// keys
	removeAllKeys(*store, TRUE);	// patterns

	/* Remove/cancel any outstanding notification requests. */
	__MACH_PORT_DEBUG(storePrivate->notifyPort != MACH_PORT_NULL, "*** __SCDynamicStoreClose", storePrivate->notifyPort);
	(void) __SCDynamicStoreNotifyCancel(*store);

	/* Remove any session keys */
	mySession = getSession(storePrivate->server);
	if (mySession->sessionKeys != NULL) {
		CFIndex		n	= CFArrayGetCount(mySession->sessionKeys);
		Boolean		push	= FALSE;
		CFStringRef	sessionKey;

		sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), storePrivate->server);
		for (CFIndex i = 0; i < n; i++) {
			CFStringRef	key	= CFArrayGetValueAtIndex(mySession->sessionKeys, i);

			if (isMySessionKey(sessionKey, key)) {
				(void) __SCDynamicStoreRemoveValue(*store, key, TRUE);
				push = TRUE;
			}
		}
		CFRelease(sessionKey);

		if (push) {
			/* push changes */
			(void) __SCDynamicStorePush();
		}
	}

	storePrivate->server = MACH_PORT_NULL;

	CFRelease(*store);
	*store = NULL;

	return kSCStatusOK;
}
