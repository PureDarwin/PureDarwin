/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSBundle_h
#define NSBundle_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSArray, NSDictionary, NSString, NSURL;

@interface NSBundle : NSObject

+ (NSBundle *)mainBundle;
+ (NSBundle *)bundleWithPath:(NSString *)path;
+ (NSBundle *)bundleForClass:(Class)aClass;

- (instancetype)initWithPath:(NSString *)path;

- (NSString *)bundlePath;
- (NSURL *)bundleURL;
- (NSString *)bundleIdentifier;
- (NSString *)resourcePath;
- (NSDictionary *)infoDictionary;
- (id)objectForInfoDictionaryKey:(NSString *)key;

- (NSString *)pathForResource:(NSString *)name ofType:(NSString *)extension;
- (NSString *)pathForResource:(NSString *)name
                       ofType:(NSString *)extension
                  inDirectory:(NSString *)subpath;

@end

#endif /* NSBundle_h */
