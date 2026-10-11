/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSURL.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSNumber.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <objc/runtime.h>

@implementation NSURL

/* Immutable and with no mutable counterpart, so a copy is the object itself.
 * Without this -copy raised "unrecognized selector". */
- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

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

+ (instancetype)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDirectory {
    return [(id)CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path,
                                              kCFURLPOSIXPathStyle, isDirectory) autorelease];
}

- (nullable NSURL *)absoluteURL {
    return [(id)CFURLCopyAbsoluteURL((CFURLRef)self) autorelease];
}

- (nullable NSString *)lastPathComponent {
    return [(id)CFURLCopyLastPathComponent((CFURLRef)self) autorelease];
}

- (nullable NSString *)pathExtension {
    CFStringRef ext = CFURLCopyPathExtension((CFURLRef)self);

    return ext ? [(id)ext autorelease] : @"";
}

- (nullable NSURL *)URLByAppendingPathComponent:(NSString *)component {
    return [self URLByAppendingPathComponent:component isDirectory:[component hasSuffix:@"/"]];
}

- (nullable NSURL *)URLByAppendingPathComponent:(NSString *)component isDirectory:(BOOL)isDirectory {
    return [(id)CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, (CFURLRef)self,
                                                      (CFStringRef)component, isDirectory) autorelease];
}

- (nullable NSURL *)URLByDeletingLastPathComponent {
    return [(id)CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, (CFURLRef)self) autorelease];
}

- (nullable NSURL *)URLByAppendingPathExtension:(NSString *)extension {
    return [(id)CFURLCreateCopyAppendingPathExtension(kCFAllocatorDefault, (CFURLRef)self,
                                                      (CFStringRef)extension) autorelease];
}

- (nullable NSURL *)URLByDeletingPathExtension {
    return [(id)CFURLCreateCopyDeletingPathExtension(kCFAllocatorDefault, (CFURLRef)self) autorelease];
}

// "." and ".." removed from a file URL's path, other URLs come back as they are
- (nullable NSURL *)standardizedURL {
    if (![self isFileURL])
        return [[self copy] autorelease];

    NSMutableArray *out = [NSMutableArray array];
    NSString *path = [self path];

    for (NSString *c in [path componentsSeparatedByString:@"/"]) {
        if ([c length] == 0 || [c isEqualToString:@"."])
            continue;
        if ([c isEqualToString:@".."]) {
            if ([out count])
                [out removeLastObject];
            continue;
        }
        [out addObject:c];
    }
    NSString *clean = [@"/" stringByAppendingString:[out componentsJoinedByString:@"/"]];
    return [NSURL fileURLWithPath:clean isDirectory:[[self absoluteString] hasSuffix:@"/"]];
}

- (const char *)fileSystemRepresentation {
    CFStringRef path = CFURLCopyFileSystemPath((CFURLRef)self, kCFURLPOSIXPathStyle);

    return path ? [[(id)path autorelease] fileSystemRepresentation] : NULL;
}

/* nil when the URL carries no explicit port, matching Foundation. */
- (NSNumber *)port {
    SInt32 port = CFURLGetPortNumber((CFURLRef)self);

    return (port < 0) ? nil : [NSNumber numberWithInt:(int)port];
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFURLBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFURLGetTypeID(), "NSURL");
}
#endif
