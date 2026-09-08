/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSLocale.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/ForFoundationOnly.h>

NSLocaleKey const NSLocaleIdentifier = @"kCFLocaleIdentifierKey";
NSLocaleKey const NSLocaleLanguageCode = @"kCFLocaleLanguageCodeKey";
NSLocaleKey const NSLocaleCountryCode = @"kCFLocaleCountryCodeKey";
NSLocaleKey const NSLocaleDecimalSeparator = @"kCFLocaleDecimalSeparatorKey";
NSLocaleKey const NSLocaleGroupingSeparator = @"kCFLocaleGroupingSeparatorKey";

/* Bridged onto CFLocale, so instances are CF objects. */
@implementation NSLocale

+ (NSLocale *)currentLocale {
    return (NSLocale *)CFAutorelease(CFLocaleCopyCurrent());
}

+ (NSLocale *)systemLocale {
    return (NSLocale *)CFLocaleGetSystem();
}

+ (NSArray *)preferredLanguages {
    return (NSArray *)CFAutorelease(CFLocaleCopyPreferredLanguages());
}

- (instancetype)initWithLocaleIdentifier:(NSString *)identifier {
    return (id)CFLocaleCreate(kCFAllocatorDefault, (CFStringRef)identifier);
}

- (NSString *)localeIdentifier {
    return (NSString *)CFLocaleGetIdentifier((CFLocaleRef)self);
}

- (id)objectForKey:(NSLocaleKey)key {
    return (id)CFLocaleGetValue((CFLocaleRef)self, (CFStringRef)key);
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFRetain((CFTypeRef)self);
}

- (id)retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFLocaleBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFLocaleGetTypeID(), "NSLocale");
}
#endif
