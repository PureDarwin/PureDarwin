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
@class NSMutableDictionary;

@interface NSKeyedUnarchiver : NSCoder
{
@public
    NSData *_data;
    NSMutableArray *_containers;
    id _root;
    NSMutableDictionary *_classes;
    id _delegate;
}

+ (id)unarchiveObjectWithData:(NSData *)data;
+ (id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error;

+ (void)setClass:(Class)cls forClassName:(NSString *)name;
+ (Class)classForClassName:(NSString *)name;

- (void)setClass:(Class)cls forClassName:(NSString *)name;
- (Class)classForClassName:(NSString *)name;
- (void)setDelegate:(id)delegate;
- (id)delegate;

- (id)initForReadingWithData:(NSData *)data;
- (id)initForReadingFromData:(NSData *)data error:(NSError **)error;

- (void)finishDecoding;

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length;

@end

#endif /* ! __FOUNDATION_NSKEYEDUNARCHIVER__ */
