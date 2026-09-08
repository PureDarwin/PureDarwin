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

@class NSData, NSMutableArray, NSError;

/* Decoding half of the property-list-backed keyed archive format. */
@interface NSKeyedUnarchiver : NSCoder
{
@public
    NSData *_data;
    NSMutableArray *_containers;
    id _root;
}

+ (id)unarchiveObjectWithData:(NSData *)data;
+ (id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error;

- (id)initForReadingFromData:(NSData *)data error:(NSError **)error;

- (void)finishDecoding;

@end

#endif /* ! __FOUNDATION_NSKEYEDUNARCHIVER__ */
