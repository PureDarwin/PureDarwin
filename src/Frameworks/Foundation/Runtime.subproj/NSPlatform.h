/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPlatform_h
#define NSPlatform_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray;

/* Cocotron's per-OS hook object. PureDarwin has one platform, so this is a
 * single shared instance answering the directory questions callers ask. */
FOUNDATION_EXPORT NSString * const NSPlatformResourceNameSuffix;

@interface NSPlatform : NSObject

+ (NSPlatform *)currentPlatform;

- (NSString *)libraryDirectory;
- (NSString *)userName;
- (NSString *)homeDirectory;
- (NSArray *)arguments;

@end

#endif /* NSPlatform_h */
