/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSDictionary_h
#define NSDictionary_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSArray.h>

@class NSURL, NSError, NSString;

@interface NSDictionary<__covariant KeyType, __covariant ObjectType> : NSObject

+ (instancetype)dictionary;
/* The compiler emits +dictionaryWithObjects:forKeys:count: for a @{...}
 * literal, so it is API rather than convenience. */
+ (instancetype)dictionaryWithObjects:(const ObjectType _Nonnull [_Nullable])objects
                              forKeys:(const KeyType _Nonnull [_Nullable])keys
                                count:(NSUInteger)count;

/* The plist readers. -contentsOfURL: is what NSProcessInfo-free code uses to
 * read SystemVersion.plist and friends; both go through CFPropertyList. */
+ (nullable instancetype)dictionaryWithContentsOfURL:(NSURL *)url
                                               error:(NSError **)error;
+ (nullable instancetype)dictionaryWithContentsOfFile:(NSString *)path;

- (NSUInteger)count;
- (nullable id)objectForKey:(id)key;
- (nullable id)objectForKeyedSubscript:(id)key;
- (NSArray<KeyType> *)allKeys;
- (void)enumerateKeysAndObjectsUsingBlock:(void (^)(KeyType key, ObjectType obj, BOOL *stop))block;

@end

@interface NSMutableDictionary<KeyType, ObjectType> : NSDictionary<KeyType, ObjectType>

+ (instancetype)dictionaryWithCapacity:(NSUInteger)capacity;

- (void)setObject:(id)object forKey:(id)key;
- (void)setObject:(id)object forKeyedSubscript:(id)key;
- (void)removeObjectForKey:(id)key;

@end

#endif /* NSDictionary_h */
