/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

static NSString *_string(const char *cString) {
    if (cString == NULL) {
        return nil;
    }
    return (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                 kCFStringEncodingUTF8);
}

static struct passwd *_currentPasswd(void) {
    return getpwuid(getuid());
}

NSString *NSUserName(void) {
    struct passwd *pw = _currentPasswd();
    return pw != NULL ? _string(pw->pw_name) : _string("");
}

NSString *NSFullUserName(void) {
    struct passwd *pw = _currentPasswd();
    if (pw == NULL || pw->pw_gecos == NULL || pw->pw_gecos[0] == '\0') {
        return NSUserName();
    }
    return _string(pw->pw_gecos);
}

NSString *NSHomeDirectory(void) {
    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        return _string(home);
    }

    struct passwd *pw = _currentPasswd();
    return pw != NULL ? _string(pw->pw_dir) : _string("/");
}

NSString *NSHomeDirectoryForUser(NSString *userName) {
    char buffer[256];
    if (!CFStringGetCString((CFStringRef)userName, buffer, sizeof(buffer),
                            kCFStringEncodingUTF8)) {
        return nil;
    }

    struct passwd *pw = getpwnam(buffer);
    return pw != NULL ? _string(pw->pw_dir) : nil;
}

NSString *NSTemporaryDirectory(void) {
    const char *tmp = getenv("TMPDIR");
    return _string(tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
}

NSString *NSOpenStepRootDirectory(void) {
    return _string("/");
}

/* Path arithmetic goes through CFURL where it can, so the edge cases (trailing
 * slashes, "/" itself, extension-less names) match CoreFoundation's. */

@implementation NSString (NSPathUtilities)

- (NSString *)lastPathComponent {
    NSArray<NSString *> *components = [self pathComponents];
    NSUInteger count = [components count];

    if (count == 0) {
        return (NSString *)CFSTR("");
    }
    return [components objectAtIndex:count - 1];
}

- (NSString *)stringByDeletingLastPathComponent {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)self,
                                                 kCFURLPOSIXPathStyle, false);
    if (url == NULL) {
        return self;
    }

    CFURLRef parent = CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, url);
    CFRelease(url);
    if (parent == NULL) {
        return self;
    }

    CFStringRef path = CFURLCopyFileSystemPath(parent, kCFURLPOSIXPathStyle);
    CFRelease(parent);
    return (NSString *)path;
}

- (NSString *)pathExtension {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)self,
                                                 kCFURLPOSIXPathStyle, false);
    if (url == NULL) {
        return (NSString *)CFSTR("");
    }

    CFStringRef extension = CFURLCopyPathExtension(url);
    CFRelease(url);
    return extension != NULL ? (NSString *)extension : (NSString *)CFSTR("");
}

- (NSString *)stringByDeletingPathExtension {
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)self,
                                                 kCFURLPOSIXPathStyle, false);
    if (url == NULL) {
        return self;
    }

    CFURLRef stripped = CFURLCreateCopyDeletingPathExtension(kCFAllocatorDefault, url);
    CFRelease(url);
    if (stripped == NULL) {
        return self;
    }

    CFStringRef path = CFURLCopyFileSystemPath(stripped, kCFURLPOSIXPathStyle);
    CFRelease(stripped);
    return (NSString *)path;
}

- (NSString *)stringByAppendingPathComponent:(NSString *)component {
    CFMutableStringRef path = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    if (CFStringGetLength(path) > 0 && !CFStringHasSuffix(path, CFSTR("/"))) {
        CFStringAppend(path, CFSTR("/"));
    }
    CFStringAppend(path, (CFStringRef)component);

    return (NSString *)path;
}

- (NSString *)stringByAppendingPathExtension:(NSString *)extension {
    CFMutableStringRef path = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);
    CFStringAppend(path, CFSTR("."));
    CFStringAppend(path, (CFStringRef)extension);
    return (NSString *)path;
}

- (NSArray<NSString *> *)pathComponents {
    NSMutableArray<NSString *> *components = [NSMutableArray arrayWithCapacity:0];
    CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault,
                                                              (CFStringRef)self,
                                                              CFSTR("/"));
    if (parts == NULL) {
        return components;
    }

    if ([self isAbsolutePath]) {
        [components addObject:(NSString *)CFSTR("/")];
    }

    CFIndex count = CFArrayGetCount(parts);
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef part = CFArrayGetValueAtIndex(parts, i);
        if (CFStringGetLength(part) > 0) {
            [components addObject:(__bridge NSString *)part];
        }
    }
    CFRelease(parts);

    return components;
}

- (BOOL)isAbsolutePath {
    return CFStringHasPrefix((CFStringRef)self, CFSTR("/")) ? YES : NO;
}

@end
