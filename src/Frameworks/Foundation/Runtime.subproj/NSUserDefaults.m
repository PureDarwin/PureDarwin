/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSData.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#include <CoreFoundation/CFPreferences.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

/* Backed by CFPreferences for the current application; -registerDefaults keeps
 * its own volatile layer, consulted only when CFPreferences has no value. */

NSString * const NSUserDefaultsDidChangeNotification = @"NSUserDefaultsDidChangeNotification";

NSString * const NSGlobalDomain = @"NSGlobalDomain";
NSString * const NSArgumentDomain = @"NSArgumentDomain";
NSString * const NSRegistrationDomain = @"NSRegistrationDomain";

/* A domain name is a CFPreferences application ID. NSGlobalDomain is the
 * cross-application domain, which CF spells kCFPreferencesAnyApplication. */
static CFStringRef _applicationIDForDomain(NSString *domainName) {
    if (domainName == nil || [domainName isEqualToString:NSGlobalDomain]) {
        return kCFPreferencesAnyApplication;
    }
    return (CFStringRef)domainName;
}

@implementation NSUserDefaults {
    NSMutableDictionary *_registered;
    NSMutableDictionary *_volatile;
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
        _volatile = [NSMutableDictionary dictionaryWithCapacity:0];
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

- (NSDictionary *)persistentDomainForName:(NSString *)domainName {
    CFStringRef appID = _applicationIDForDomain(domainName);
    CFArrayRef keys = CFPreferencesCopyKeyList(appID, kCFPreferencesCurrentUser,
                                               kCFPreferencesAnyHost);
    if (keys == NULL) {
        return nil;
    }

    NSMutableDictionary *domain = [NSMutableDictionary dictionaryWithCapacity:0];
    CFIndex count = CFArrayGetCount(keys);

    for (CFIndex i = 0; i < count; i++) {
        CFStringRef key = CFArrayGetValueAtIndex(keys, i);
        CFPropertyListRef value = CFPreferencesCopyValue(key, appID,
                                                         kCFPreferencesCurrentUser,
                                                         kCFPreferencesAnyHost);
        if (value != NULL) {
            [domain setObject:(id)value forKey:(NSString *)key];
            CFRelease(value);
        }
    }
    CFRelease(keys);
    return domain;
}

- (void)setPersistentDomain:(NSDictionary *)domain forName:(NSString *)domainName {
    [self removePersistentDomainForName:domainName];

    CFStringRef appID = _applicationIDForDomain(domainName);

    for (NSString *key in [domain allKeys]) {
        CFPreferencesSetValue((CFStringRef)key,
                              (CFPropertyListRef)[domain objectForKey:key],
                              appID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    }
    CFPreferencesSynchronize(appID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

- (void)removePersistentDomainForName:(NSString *)domainName {
    CFStringRef appID = _applicationIDForDomain(domainName);
    CFArrayRef keys = CFPreferencesCopyKeyList(appID, kCFPreferencesCurrentUser,
                                               kCFPreferencesAnyHost);
    if (keys == NULL) {
        return;
    }

    CFIndex count = CFArrayGetCount(keys);

    for (CFIndex i = 0; i < count; i++) {
        CFPreferencesSetValue(CFArrayGetValueAtIndex(keys, i), NULL, appID,
                              kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    }
    CFRelease(keys);
    CFPreferencesSynchronize(appID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

- (NSArray *)persistentDomainNames {
    /* CFPreferences exposes no way to enumerate application IDs, so this
     * reports only the domains this process can name. */
    return [NSArray arrayWithObjects:NSGlobalDomain, nil];
}

- (NSDictionary *)volatileDomainForName:(NSString *)domainName {
    if ([domainName isEqualToString:NSRegistrationDomain]) {
        return _registered;
    }
    return [_volatile objectForKey:domainName];
}

- (void)setVolatileDomain:(NSDictionary *)domain forName:(NSString *)domainName {
    if ([domainName isEqualToString:NSRegistrationDomain]) {
        [_registered removeAllObjects];
        [_registered addEntriesFromDictionary:domain];
        return;
    }
    [_volatile setObject:domain forKey:domainName];
}

- (void)removeVolatileDomainForName:(NSString *)domainName {
    if ([domainName isEqualToString:NSRegistrationDomain]) {
        [_registered removeAllObjects];
        return;
    }
    [_volatile removeObjectForKey:domainName];
}

- (NSArray *)volatileDomainNames {
    NSMutableArray *names = [NSMutableArray arrayWithArray:[_volatile allKeys]];
    [names addObject:NSRegistrationDomain];
    return names;
}

/* Search order: registration domain first, then the application domain on top,
 * matching how -objectForKey: resolves a single key. */
- (NSDictionary *)dictionaryRepresentation {
    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:0];

    [result addEntriesFromDictionary:_registered];

    NSDictionary *app = [self persistentDomainForName:
        (NSString *)kCFPreferencesCurrentApplication];
    if (app != nil) {
        [result addEntriesFromDictionary:app];
    }
    return result;
}

- (id)objectForKey:(NSString *)key inDomain:(NSString *)domainName {
    CFPropertyListRef value = CFPreferencesCopyValue((CFStringRef)key,
                                                     _applicationIDForDomain(domainName),
                                                     kCFPreferencesCurrentUser,
                                                     kCFPreferencesAnyHost);
    return value != NULL ? [(id)value autorelease] : nil;
}

- (void)setObject:(id)value forKey:(NSString *)key inDomain:(NSString *)domainName {
    CFStringRef appID = _applicationIDForDomain(domainName);

    CFPreferencesSetValue((CFStringRef)key, (CFPropertyListRef)value, appID,
                          kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize(appID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

@end
