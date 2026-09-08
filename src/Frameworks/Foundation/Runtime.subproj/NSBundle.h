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

- (NSString *)localizedStringForKey:(NSString *)key
                              value:(NSString *)value
                              table:(NSString *)tableName;
- (NSString *)pathForResource:(NSString *)name
                       ofType:(NSString *)extension
                  inDirectory:(NSString *)subpath;
- (NSArray *)pathsForResourcesOfType:(NSString *)extension
                         inDirectory:(NSString *)subpath;
- (Class)principalClass;

@end

/* The lookup macros. Localisation tables are not read yet, so these resolve to
 * the key itself - correct for an unlocalised build, and the call sites stay
 * source compatible for when tables land. */
#define NSLocalizedString(key, comment) \
    [[NSBundle mainBundle] localizedStringForKey:(key) value:@"" table:nil]

#define NSLocalizedStringFromTable(key, tbl, comment) \
    [[NSBundle mainBundle] localizedStringForKey:(key) value:@"" table:(tbl)]

#define NSLocalizedStringFromTableInBundle(key, tbl, bundle, comment) \
    [(bundle) localizedStringForKey:(key) value:@"" table:(tbl)]

#define NSLocalizedStringWithDefaultValue(key, tbl, bundle, val, comment) \
    [(bundle) localizedStringForKey:(key) value:(val) table:(tbl)]

#endif /* NSBundle_h */