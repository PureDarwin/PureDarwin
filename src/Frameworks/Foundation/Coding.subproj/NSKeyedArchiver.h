/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSKeyedArchiver_h
#define NSKeyedArchiver_h

#import <Foundation/NSCoder.h>
#import <Foundation/NSKeyedUnarchiver.h>

@class NSData, NSMutableArray, NSMutableData, NSError;

/* Keyed coding backed by a binary property-list representation. */
@interface NSKeyedArchiver : NSCoder
{
@public
    NSMutableData *_data;
    NSMutableArray *_containers;
    id _root;
    BOOL _requiresSecureCoding;
}

+ (NSData *)archivedDataWithRootObject:(id)object;
+ (NSData *)archivedDataWithRootObject:(id)object
                 requiringSecureCoding:(BOOL)secure
                                 error:(NSError **)error;

- (instancetype)initRequiringSecureCoding:(BOOL)secure;
- (instancetype)initForWritingWithMutableData:(NSMutableData *)data;

- (void)finishEncoding;
- (NSData *)encodedData;

@end

#endif /* NSKeyedArchiver_h */
