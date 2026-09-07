/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#if !defined(__FOUNDATION_NSKEYEDUNARCHIVER__)
#define __FOUNDATION_NSKEYEDUNARCHIVER__ 1

#import <Foundation/NSCoder.h>

@class NSData, NSError;

/* Declared so sources that decode keyed archives compile and link. The keyed
 * archive format itself is not implemented yet - every entry point raises. */
@interface NSKeyedUnarchiver : NSCoder

+ (id)unarchiveObjectWithData:(NSData *)data;
+ (id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error;

- (id)initForReadingFromData:(NSData *)data error:(NSError **)error;

- (void)finishDecoding;

@end

#endif /* ! __FOUNDATION_NSKEYEDUNARCHIVER__ */
