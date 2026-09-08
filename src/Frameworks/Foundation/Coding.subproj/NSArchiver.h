/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSArchiver_h
#define NSArchiver_h

#import <Foundation/NSCoder.h>

@class NSData, NSMutableData;

/* The pre-keyed archivers. Declared so sources that mention them compile and
 * link; the sequential archive format is not implemented and every entry
 * point raises. New code should use NSKeyedArchiver. */
@interface NSArchiver : NSCoder

+ (NSData *)archivedDataWithRootObject:(id)root;
+ (BOOL)archiveRootObject:(id)root toFile:(NSString *)path;

- (instancetype)initForWritingWithMutableData:(NSMutableData *)data;
- (NSMutableData *)archiverData;

@end

@interface NSUnarchiver : NSCoder

+ (id)unarchiveObjectWithData:(NSData *)data;
+ (id)unarchiveObjectWithFile:(NSString *)path;

- (instancetype)initForReadingWithData:(NSData *)data;
- (BOOL)isAtEnd;

@end

#endif /* NSArchiver_h */
