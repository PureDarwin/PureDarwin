/*
 * Copyright (c) 2000-2005, 2009, 2012, 2016, 2017, 2019-2021 Apple Inc. All rights reserved.
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

#ifndef _SCUTIL_H
#define _SCUTIL_H

#include <sys/cdefs.h>
#include <histedit.h>

#define SC_LOG_HANDLE		_SC_LOG_DEFAULT

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>


typedef struct {
	FILE		*fp;
	EditLine	*el;
	History		*h;
} Input, *InputRef;


extern AuthorizationRef		authorization;
extern InputRef			currentInput;
extern Boolean			doDispatch;
extern int			nesting;
extern SCPreferencesRef		ni_prefs;
extern CFRunLoopRef		notifyRl;
extern CFRunLoopSourceRef	notifyRls;
extern SCPreferencesRef		prefs;
extern char			*prefsPath;
extern SCDynamicStoreRef	store;
extern CFPropertyListRef	value;
extern CFMutableArrayRef	watchedKeys;
extern CFMutableArrayRef	watchedPatterns;


__BEGIN_DECLS

Boolean		process_line		(InputRef			src);

CFStringRef	_copyStringFromSTDIN	(CFStringRef			prompt,
					 CFStringRef			defaultValue);

Boolean		get_bool_from_string	(const char			*str,
					 Boolean			def_value,
					 Boolean			*ret_value,
					 Boolean			*use_default);

Boolean		get_rank_from_string	(const char			*str,
					 SCNetworkServicePrimaryRank	*ret_rank);

__END_DECLS

#endif /* !_SCUTIL_H */
