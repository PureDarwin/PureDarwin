/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSData.h>
#import <Foundation/NSNumber.h>
#include <CoreFoundation/CFPreferences.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

/* Backed by CFPreferences for the current application; -registerDefaults keeps
 * its own volatile layer, consulted only when CFPreferences has no value. */

@implementation NSUserDefaults {
    NSMutableDictionary *_registered;
}

+ (NSUserDefaults *)standardUserDefaults {
    static NSUserDefaults *shared = nil;
    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

+ (void)resetStandardUserDefaults {
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _registered = [NSMutableDictionary dictionaryWithCapacity:0];
    }
    return self;
}

- (id)objectForKey:(NSString *)key {
    CFPropertyListRef value = CFPreferencesCopyAppValue((CFStringRef)key,
                                                        kCFPreferencesCurrentApplication);
    if (value != NULL) {
        return (__bridge id)value;
    }
    return [_registered objectForKey:key];
}

- (void)setObject:(id)value forKey:(NSString *)key {
    CFPreferencesSetAppValue((CFStringRef)key, (__bridge CFPropertyListRef)value,
                             kCFPreferencesCurrentApplication);
}

- (void)removeObjectForKey:(NSString *)key {
    CFPreferencesSetAppValue((CFStringRef)key, NULL, kCFPreferencesCurrentApplication);
}

- (NSString *)stringForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value isKindOfClass:[NSString class]] ? value : nil;
}

- (NSArray *)arrayForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value isKindOfClass:[NSArray class]] ? value : nil;
}

- (NSDictionary *)dictionaryForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value isKindOfClass:[NSDictionary class]] ? value : nil;
}

- (NSData *)dataForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value isKindOfClass:[NSData class]] ? value : nil;
}

- (NSInteger)integerForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value respondsToSelector:@selector(integerValue)] ? [value integerValue] : 0;
}

- (float)floatForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value respondsToSelector:@selector(floatValue)] ? [value floatValue] : 0;
}

- (double)doubleForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value respondsToSelector:@selector(doubleValue)] ? [value doubleValue] : 0;
}

- (BOOL)boolForKey:(NSString *)key {
    id value = [self objectForKey:key];
    return [value respondsToSelector:@selector(boolValue)] ? [value boolValue] : NO;
}

- (void)setInteger:(NSInteger)value forKey:(NSString *)key {
    [self setObject:[NSNumber numberWithLongLong:(long long)value] forKey:key];
}

- (void)setFloat:(float)value forKey:(NSString *)key {
    [self setObject:[NSNumber numberWithDouble:(double)value] forKey:key];
}

- (void)setDouble:(double)value forKey:(NSString *)key {
    [self setObject:[NSNumber numberWithDouble:value] forKey:key];
}

- (void)setBool:(BOOL)value forKey:(NSString *)key {
    [self setObject:[NSNumber numberWithBool:value] forKey:key];
}

- (void)registerDefaults:(NSDictionary *)defaults {
    [_registered addEntriesFromDictionary:defaults];
}

- (BOOL)synchronize {
    return CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication) ? YES : NO;
}

@end
