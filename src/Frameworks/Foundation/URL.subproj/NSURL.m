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

extern Boolean _CFIsObjC(CFTypeID typeID, void *obj);

@implementation NSURL

/* Immutable and with no mutable counterpart, so a copy is the object itself.
 * Without this -copy raised "unrecognized selector". */
- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

+ (nullable instancetype)URLWithString:(NSString *)string {
    return [[[self alloc] initWithString:string] autorelease];
}

+ (instancetype)fileURLWithPath:(NSString *)path {
    return [(id)CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                              (CFStringRef)path,
                                              kCFURLPOSIXPathStyle, false) autorelease];
}

- (nullable instancetype)initWithString:(NSString *)string {
    return (id)CFURLCreateWithString(kCFAllocatorDefault, (CFStringRef)string, NULL);
}

- (nullable NSString *)absoluteString {
    return (NSString *)CFURLGetString((CFURLRef)self);
}

- (nullable NSString *)path {
    return [(NSString *)CFURLCopyFileSystemPath((CFURLRef)self, kCFURLPOSIXPathStyle) autorelease];
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

- (nullable NSString *)scheme {
    return [(NSString *)CFURLCopyScheme((CFURLRef)self) autorelease];
}

- (nullable NSString *)host {
    return [(NSString *)CFURLCopyHostName((CFURLRef)self) autorelease];
}

- (nullable NSString *)user {
    return [(NSString *)CFURLCopyUserName((CFURLRef)self) autorelease];
}

- (nullable NSString *)password {
    return [(NSString *)CFURLCopyPassword((CFURLRef)self) autorelease];
}

- (nullable NSString *)query {
    return [(NSString *)CFURLCopyQueryString((CFURLRef)self, NULL) autorelease];
}

- (nullable NSString *)fragment {
    return [(NSString *)CFURLCopyFragment((CFURLRef)self, NULL) autorelease];
}

- (nullable NSString *)resourceSpecifier {
    return [(NSString *)CFURLCopyResourceSpecifier((CFURLRef)self) autorelease];
}

- (NSString *)relativeString {
    return (NSString *)CFURLGetString((CFURLRef)self);
}

- (nullable NSURL *)baseURL {
    return (NSURL *)CFURLGetBaseURL((CFURLRef)self);
}

+ (nullable instancetype)URLWithString:(NSString *)string relativeToURL:(NSURL *)base {
    return [[[self alloc] initWithString:string relativeToURL:base] autorelease];
}

- (nullable instancetype)initWithString:(NSString *)string relativeToURL:(NSURL *)base {
    return (id)CFURLCreateWithString(kCFAllocatorDefault, (CFStringRef)string, (CFURLRef)base);
}

- (instancetype)initFileURLWithPath:(NSString *)path {
    return (id)CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path, kCFURLPOSIXPathStyle, [path hasSuffix:@"/"]);
}

- (instancetype)initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDirectory {
    return (id)CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path, kCFURLPOSIXPathStyle, isDirectory);
}

- (BOOL)hasDirectoryPath {
    return CFURLHasDirectoryPath((CFURLRef)self);
}

- (nullable NSArray *)pathComponents {
    NSString *path = [self path];
    NSMutableArray *components = [NSMutableArray array];

    if (path == nil)
        return nil;
    if ([path hasPrefix:@"/"])
        [components addObject:@"/"];
    for (NSString *part in [path componentsSeparatedByString:@"/"]) {
        if ([part length])
            [components addObject:part];
    }
    return components;
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSURL class]] || _CFIsObjC(CFURLGetTypeID(), (void *)self) || _CFIsObjC(CFURLGetTypeID(), (void *)other))
        return [super isEqual:other];
    return CFEqual((CFTypeRef)self, (CFTypeRef)other);
}

- (NSUInteger)hash {
    if (_CFIsObjC(CFURLGetTypeID(), (void *)self))
        return [super hash];
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (NSString *)description {
    return [self absoluteString];
}

/* nil when the URL carries no explicit port, matching Foundation. */
- (NSNumber *)port {
    SInt32 port = CFURLGetPortNumber((CFURLRef)self);

    return (port < 0) ? nil : [NSNumber numberWithInt:(int)port];
}


// CF owns the count of objects CF made, NSObject's would let CFRelease free them under a retainer.
// Instances of ObjC subclasses keep NSObject's count
- (id)retain {
    if (_CFIsObjC(CFURLGetTypeID(), (void *)self))
        return [super retain];
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    if (_CFIsObjC(CFURLGetTypeID(), (void *)self)) {
        [super release];
        return;
    }
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    if (_CFIsObjC(CFURLGetTypeID(), (void *)self))
        return [super retainCount];
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}
@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFURLBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFURLGetTypeID(), "NSURL");
}
#endif
