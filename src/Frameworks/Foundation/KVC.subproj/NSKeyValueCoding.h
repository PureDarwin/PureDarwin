/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSKeyValueCoding_h
#define NSKeyValueCoding_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray, NSDictionary, NSError;

FOUNDATION_EXPORT NSString *const NSUndefinedKeyException;

@interface NSObject (NSKeyValueCoding)

+ (BOOL)accessInstanceVariablesDirectly;

- (id)valueForKey:(NSString *)key;
- (void)setValue:(id)value forKey:(NSString *)key;

- (id)valueForKeyPath:(NSString *)keyPath;
- (void)setValue:(id)value forKeyPath:(NSString *)keyPath;

- (id)valueForUndefinedKey:(NSString *)key;
- (void)setValue:(id)value forUndefinedKey:(NSString *)key;
- (void)setNilValueForKey:(NSString *)key;

- (NSDictionary *)dictionaryWithValuesForKeys:(NSArray *)keys;
- (void)setValuesForKeysWithDictionary:(NSDictionary *)values;

@end

#endif /* NSKeyValueCoding_h */
