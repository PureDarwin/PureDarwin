/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSFileManager_h
#define NSFileManager_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

@class NSData, NSDictionary, NSError;

FOUNDATION_EXPORT NSString *const NSFileSize;
FOUNDATION_EXPORT NSString *const NSFileType;
FOUNDATION_EXPORT NSString *const NSFileTypeRegular;
FOUNDATION_EXPORT NSString *const NSFileTypeDirectory;
FOUNDATION_EXPORT NSString *const NSFileTypeSymbolicLink;
FOUNDATION_EXPORT NSString *const NSFileTypeUnknown;

@interface NSFileManager : NSObject

+ (NSFileManager *)defaultManager;

- (BOOL)fileExistsAtPath:(NSString *)path;
- (BOOL)fileExistsAtPath:(NSString *)path isDirectory:(BOOL *)isDirectory;
- (BOOL)isReadableFileAtPath:(NSString *)path;
- (BOOL)isWritableFileAtPath:(NSString *)path;
- (BOOL)isExecutableFileAtPath:(NSString *)path;

- (NSArray<NSString *> *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error;
- (NSDictionary *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error;

- (NSData *)contentsAtPath:(NSString *)path;
- (BOOL)createFileAtPath:(NSString *)path
                contents:(NSData *)contents
              attributes:(NSDictionary *)attributes;
- (BOOL)createDirectoryAtPath:(NSString *)path
  withIntermediateDirectories:(BOOL)createIntermediates
                   attributes:(NSDictionary *)attributes
                        error:(NSError **)error;
- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error;

- (NSString *)currentDirectoryPath;
- (BOOL)changeCurrentDirectoryPath:(NSString *)path;

@end

#endif /* NSFileManager_h */
