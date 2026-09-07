/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSURL.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <objc/runtime.h>

@implementation NSURL

+ (nullable instancetype)URLWithString:(NSString *)string {
    return [[self alloc] initWithString:string];
}

+ (instancetype)fileURLWithPath:(NSString *)path {
    return (id)CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                             (CFStringRef)path,
                                             kCFURLPOSIXPathStyle, false);
}

- (nullable instancetype)initWithString:(NSString *)string {
    return (id)CFURLCreateWithString(kCFAllocatorDefault, (CFStringRef)string, NULL);
}

- (nullable NSString *)absoluteString {
    return (NSString *)CFURLGetString((CFURLRef)self);
}

- (nullable NSString *)path {
    return (NSString *)CFURLCopyFileSystemPath((CFURLRef)self, kCFURLPOSIXPathStyle);
}

- (BOOL)isFileURL {
    CFStringRef scheme = CFURLCopyScheme((CFURLRef)self);
    if (scheme == NULL) {
        return NO;
    }
    BOOL isFile = CFStringCompare(scheme, CFSTR("file"), 0) == kCFCompareEqualTo;
    CFRelease(scheme);
    return isFile;
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFURLBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFURLGetTypeID(), "NSURL");
}
#endif
