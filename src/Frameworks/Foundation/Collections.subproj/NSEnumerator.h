/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSEnumerator_h
#define NSEnumerator_h

#import <Foundation/NSObject.h>

@class NSArray<ObjectType>;

@interface NSEnumerator<ObjectType> : NSObject <NSFastEnumeration>

- (ObjectType)nextObject;
- (NSArray<ObjectType> *)allObjects;

@end

#endif /* NSEnumerator_h */
