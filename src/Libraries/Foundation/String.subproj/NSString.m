/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <stdarg.h>

@implementation NSString

+ (instancetype)stringWithUTF8String:(const char *)utf8String {
    return [[self alloc] initWithUTF8String:utf8String];
}

+ (instancetype)stringWithFormat:(NSString *)format, ... {
    va_list args;
    va_start(args, format);
    CFStringRef result = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, (CFStringRef)format, args);
    va_end(args);
    return (id)result;
}

- (instancetype)initWithUTF8String:(const char *)utf8String {
    CFStringRef result = CFStringCreateWithCString(kCFAllocatorDefault, utf8String, kCFStringEncodingUTF8);
    return (id)result;
}

@end
