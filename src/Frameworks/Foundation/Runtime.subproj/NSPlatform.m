/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPlatform.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#include <unistd.h>
#include <pwd.h>

NSString * const NSPlatformResourceNameSuffix = @"Darwin";

@implementation NSPlatform

+ (NSPlatform *)currentPlatform {
    static NSPlatform *shared = nil;

    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (NSString *)libraryDirectory {
    return @"/System/Library";
}

- (NSString *)homeDirectory {
    return NSHomeDirectory();
}

- (NSString *)userName {
    struct passwd *entry = getpwuid(getuid());

    if (entry != NULL && entry->pw_name != NULL) {
        return [NSString stringWithUTF8String:entry->pw_name];
    }
    return @"";
}

- (NSArray *)arguments {
    return [[NSProcessInfo processInfo] arguments];
}

@end
