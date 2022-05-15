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

CFArrayRef
SCDynamicStoreCopyKeyList(SCDynamicStoreRef store, CFStringRef pattern)
{
	SCDynamicStorePrivateRef	storePrivate;
	kern_return_t			status;
	CFDataRef			utfPattern;		/* serialized pattern */
	xmlData_t			myPatternRef;
	CFIndex				myPatternLen;
	xmlDataOut_t			xmlDataRef	= NULL;		/* serialized data */
	mach_msg_type_number_t		xmlDataLen	= 0;
	int				sc_status;
	CFArrayRef			allKeys;

	if (!__SCDynamicStoreNormalize(&store, TRUE)) {
		return NULL;
	}

	/* serialize the pattern */
	if (!_SCSerializeString(pattern, &utfPattern, &myPatternRef, &myPatternLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	storePrivate = (SCDynamicStorePrivateRef)store;

    retry :

	/* send the pattern & fetch the associated data from the server */
	status = configlist(storePrivate->server,
			    myPatternRef,
			    (mach_msg_type_number_t)myPatternLen,
			    TRUE,		/* isRegex == TRUE */
			    &xmlDataRef,
			    &xmlDataLen,
			    (int *)&sc_status);

	if (__SCDynamicStoreCheckRetryAndHandleError(store,
						     status,
						     &sc_status,
						     "SCDynamicStoreCopyKeyList configlist()")) {
		goto retry;
	}

	/* clean up */
	CFRelease(utfPattern);

	if (sc_status != kSCStatusOK) {
		if (xmlDataRef != NULL) {
			(void) vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
		}
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the list of keys */
	if (!_SCUnserialize((CFPropertyListRef *)&allKeys, NULL, xmlDataRef, xmlDataLen)) {
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	return allKeys;
}
