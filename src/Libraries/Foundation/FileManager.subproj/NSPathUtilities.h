/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPathUtilities_h
#define NSPathUtilities_h

#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSString.h>

#import <Foundation/NSArray.h>

FOUNDATION_EXPORT NSString *NSUserName(void);
FOUNDATION_EXPORT NSString *NSFullUserName(void);
FOUNDATION_EXPORT NSString *NSHomeDirectory(void);
FOUNDATION_EXPORT NSString *NSHomeDirectoryForUser(NSString *userName);
FOUNDATION_EXPORT NSString *NSTemporaryDirectory(void);
FOUNDATION_EXPORT NSString *NSOpenStepRootDirectory(void);

@interface NSString (NSPathUtilities)

- (NSString *)lastPathComponent;
- (NSString *)stringByDeletingLastPathComponent;
- (NSString *)pathExtension;
- (NSString *)stringByDeletingPathExtension;
- (NSString *)stringByAppendingPathComponent:(NSString *)component;
- (NSString *)stringByAppendingPathExtension:(NSString *)extension;
- (NSArray<NSString *> *)pathComponents;
- (BOOL)isAbsolutePath;

@end

#endif /* NSPathUtilities_h */
