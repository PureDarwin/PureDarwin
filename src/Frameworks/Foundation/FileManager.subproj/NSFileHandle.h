/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSFileHandle_h
#define NSFileHandle_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSData, NSString;

@interface NSFileHandle : NSObject

+ (NSFileHandle *)fileHandleForReadingAtPath:(NSString *)path;
+ (NSFileHandle *)fileHandleForWritingAtPath:(NSString *)path;
+ (NSFileHandle *)fileHandleForUpdatingAtPath:(NSString *)path;
+ (NSFileHandle *)fileHandleWithStandardInput;
+ (NSFileHandle *)fileHandleWithStandardOutput;
+ (NSFileHandle *)fileHandleWithStandardError;
+ (NSFileHandle *)fileHandleWithNullDevice;

- (instancetype)initWithFileDescriptor:(int)fd;
- (instancetype)initWithFileDescriptor:(int)fd closeOnDealloc:(BOOL)closeOnDealloc;

- (int)fileDescriptor;

- (NSData *)readDataToEndOfFile;
- (NSData *)readDataOfLength:(NSUInteger)length;
- (void)writeData:(NSData *)data;

- (unsigned long long)offsetInFile;
- (unsigned long long)seekToEndOfFile;
- (void)seekToFileOffset:(unsigned long long)offset;
- (void)truncateFileAtOffset:(unsigned long long)offset;

- (void)synchronizeFile;
- (void)closeFile;

@end

#endif /* NSFileHandle_h */
