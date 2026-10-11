/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSURL_h
#define NSURL_h

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>

@interface NSURL : NSObject

+ (nullable instancetype)URLWithString:(NSString *)string;
+ (instancetype)fileURLWithPath:(NSString *)path;

- (nullable instancetype)initWithString:(NSString *)string;
- (nullable NSString *)absoluteString;
- (nullable NSString *)path;
- (BOOL)isFileURL;

+ (instancetype)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDirectory;
- (nullable NSURL *)absoluteURL;
- (nullable NSString *)lastPathComponent;
- (nullable NSString *)pathExtension;
- (nullable NSURL *)URLByAppendingPathComponent:(NSString *)component;
- (nullable NSURL *)URLByAppendingPathComponent:(NSString *)component isDirectory:(BOOL)isDirectory;
- (nullable NSURL *)URLByDeletingLastPathComponent;
- (nullable NSURL *)URLByAppendingPathExtension:(NSString *)extension;
- (nullable NSURL *)URLByDeletingPathExtension;
- (nullable NSURL *)standardizedURL;
- (const char *)fileSystemRepresentation;


- (NSNumber *)port;

@end

#endif /* NSURL_h */
