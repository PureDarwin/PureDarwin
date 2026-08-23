/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _FOUNDATION_NSZONE_H_
#define _FOUNDATION_NSZONE_H_

#import <Foundation/NSObjCRuntime.h>

@class NSString;

/* Zones are vestigial on Darwin: every allocation goes to the default malloc
 * zone and the NSZone * argument is ignored. The API is kept because ported
 * ObjC code still calls it. */

NSZone *NSDefaultMallocZone(void);
NSZone *NSCreateZone(NSUInteger startSize, NSUInteger granularity, BOOL canFree);
void NSRecycleZone(NSZone *zone);
NSZone *NSZoneFromPointer(void *ptr);

void *NSZoneMalloc(NSZone *zone, NSUInteger size);
void *NSZoneCalloc(NSZone *zone, NSUInteger numElems, NSUInteger byteSize);
void *NSZoneRealloc(NSZone *zone, void *ptr, NSUInteger size);
void NSZoneFree(NSZone *zone, void *ptr);

void NSSetZoneName(NSZone *zone, NSString *name);
NSString *NSZoneName(NSZone *zone);

id NSAllocateObject(Class aClass, NSUInteger extraBytes, NSZone *zone);
void NSDeallocateObject(id object);
id NSCopyObject(id object, NSUInteger extraBytes, NSZone *zone);

#endif /* _FOUNDATION_NSZONE_H_ */
