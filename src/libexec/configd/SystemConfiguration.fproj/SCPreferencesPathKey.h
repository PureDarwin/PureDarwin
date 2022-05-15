/*
 * Copyright (c) 2004, 2005, 2008, 2018 Apple Inc. All rights reserved.
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

#ifndef _SCPREFERENCESPATHKEY_H
#define _SCPREFERENCESPATHKEY_H

#include <os/availability.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

/*!
	@header SCPreferencesPathKey
 */

__BEGIN_DECLS

/*
 * SCPreferencesPathKeyCreate*
 * - convenience routines that create a CFString key for an item in the store
 */

/*!
	@function SCPreferencesPathKeyCreate
	@discussion Creates a preferences path key using the given format.
 */
CFStringRef
SCPreferencesPathKeyCreate			(
						CFAllocatorRef	allocator,
						CFStringRef	fmt,
						...
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateNetworkServices
 */
CFStringRef
SCPreferencesPathKeyCreateNetworkServices	(
						CFAllocatorRef	allocator
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateNetworkServiceEntity
 */
CFStringRef
SCPreferencesPathKeyCreateNetworkServiceEntity	(
						CFAllocatorRef	allocator,
						CFStringRef	service,
						CFStringRef	entity
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateSets
 */
CFStringRef
SCPreferencesPathKeyCreateSets			(
						CFAllocatorRef	allocator
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateSet
 */
CFStringRef
SCPreferencesPathKeyCreateSet			(
						CFAllocatorRef	allocator,
						CFStringRef	set
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateSetNetworkInterfaceEntity
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(
						   CFAllocatorRef	allocator,
						   CFStringRef	set,
						   CFStringRef	ifname,
						   CFStringRef	entity
						   )				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateSetNetworkGlobalEntity
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkGlobalEntity(
						CFAllocatorRef	allocator,
						CFStringRef	set,
						CFStringRef	entity
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateSetNetworkService
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkService     (
						CFAllocatorRef	allocator,
						CFStringRef	set,
						CFStringRef	service
						)				API_AVAILABLE(macos(10.4), ios(2.0));

/*!
	@function SCPreferencesPathKeyCreateSetNetworkServiceEntity
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkServiceEntity(
						 CFAllocatorRef	allocator,
						 CFStringRef	set,
						 CFStringRef	service,
						 CFStringRef	entity
						 )				API_AVAILABLE(macos(10.4), ios(2.0));

__END_DECLS

#endif	/* _SCPREFERENCESPATHKEY_H */
