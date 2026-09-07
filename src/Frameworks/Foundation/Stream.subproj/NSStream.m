/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSStream.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <stdlib.h>
#include <string.h>

NSStreamPropertyKey NSStreamFileCurrentOffsetKey = nil;

/* NSStream's own methods are inherited by both subclasses, and CF keeps read
 * and write streams as separate types, so each one dispatches on the actual
 * CFTypeID of self rather than on the class. */
static Boolean __NSStreamIsRead(id self) {
    return CFGetTypeID((CFTypeRef)self) == CFReadStreamGetTypeID();
}

static CFURLRef __NSStreamCopyFileURL(NSString *path) {
    if (!path) {
        return NULL;
    }
    return CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path,
                                         kCFURLPOSIXPathStyle, false);
}

@implementation NSStream

- (void)open {
    if (__NSStreamIsRead(self)) {
        CFReadStreamOpen((CFReadStreamRef)self);
    } else {
        CFWriteStreamOpen((CFWriteStreamRef)self);
    }
}

- (void)close {
    if (__NSStreamIsRead(self)) {
        CFReadStreamClose((CFReadStreamRef)self);
    } else {
        CFWriteStreamClose((CFWriteStreamRef)self);
    }
}

- (NSStreamStatus)streamStatus {
    CFStreamStatus status = __NSStreamIsRead(self)
        ? CFReadStreamGetStatus((CFReadStreamRef)self)
        : CFWriteStreamGetStatus((CFWriteStreamRef)self);
    return (NSStreamStatus)status;
}

- (id)propertyForKey:(NSStreamPropertyKey)key {
    CFTypeRef value = __NSStreamIsRead(self)
        ? CFReadStreamCopyProperty((CFReadStreamRef)self, (CFStringRef)key)
        : CFWriteStreamCopyProperty((CFWriteStreamRef)self, (CFStringRef)key);
    return value ? (id)CFAutorelease(value) : nil;
}

- (BOOL)setProperty:(id)property forKey:(NSStreamPropertyKey)key {
    if (__NSStreamIsRead(self)) {
        return CFReadStreamSetProperty((CFReadStreamRef)self, (CFStringRef)key,
                                       (CFTypeRef)property);
    }
    return CFWriteStreamSetProperty((CFWriteStreamRef)self, (CFStringRef)key,
                                    (CFTypeRef)property);
}

@end

@implementation NSInputStream

/* CFReadStreamCreateWithBytesNoCopy does not retain the source, so the bytes
 * are copied into a malloc block that the stream frees when it is released. */
+ (instancetype)inputStreamWithData:(NSData *)data {
    if (!data) {
        return nil;
    }

    CFIndex length = CFDataGetLength((CFDataRef)data);
    UInt8 *bytes = (UInt8 *)malloc(length > 0 ? (size_t)length : 1);
    if (!bytes) {
        return nil;
    }
    if (length > 0) {
        memcpy(bytes, CFDataGetBytePtr((CFDataRef)data), (size_t)length);
    }

    CFReadStreamRef stream = CFReadStreamCreateWithBytesNoCopy(
        kCFAllocatorDefault, bytes, length, kCFAllocatorMalloc);
    if (!stream) {
        free(bytes);
        return nil;
    }
    return (id)CFAutorelease(stream);
}

+ (instancetype)inputStreamWithFileAtPath:(NSString *)path {
    CFURLRef url = __NSStreamCopyFileURL(path);
    if (!url) {
        return nil;
    }

    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    CFRelease(url);
    return stream ? (id)CFAutorelease(stream) : nil;
}

+ (instancetype)inputStreamWithURL:(NSURL *)url {
    if (!url) {
        return nil;
    }
    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault,
                                                        (CFURLRef)url);
    return stream ? (id)CFAutorelease(stream) : nil;
}

- (NSInteger)read:(uint8_t *)buffer maxLength:(NSUInteger)length {
    return (NSInteger)CFReadStreamRead((CFReadStreamRef)self, (UInt8 *)buffer,
                                       (CFIndex)length);
}

- (BOOL)hasBytesAvailable {
    return CFReadStreamHasBytesAvailable((CFReadStreamRef)self);
}

@end

@implementation NSOutputStream

+ (instancetype)outputStreamToFileAtPath:(NSString *)path append:(BOOL)shouldAppend {
    CFURLRef url = __NSStreamCopyFileURL(path);
    if (!url) {
        return nil;
    }

    NSOutputStream *stream = [self outputStreamWithURL:(NSURL *)url
                                                append:shouldAppend];
    CFRelease(url);
    return stream;
}

+ (instancetype)outputStreamWithURL:(NSURL *)url append:(BOOL)shouldAppend {
    if (!url) {
        return nil;
    }

    CFWriteStreamRef stream = CFWriteStreamCreateWithFile(kCFAllocatorDefault,
                                                          (CFURLRef)url);
    if (!stream) {
        return nil;
    }
    if (shouldAppend) {
        CFWriteStreamSetProperty(stream, kCFStreamPropertyAppendToFile,
                                 kCFBooleanTrue);
    }
    return (id)CFAutorelease(stream);
}

- (NSInteger)write:(const uint8_t *)buffer maxLength:(NSUInteger)length {
    return (NSInteger)CFWriteStreamWrite((CFWriteStreamRef)self,
                                         (const UInt8 *)buffer, (CFIndex)length);
}

- (BOOL)hasSpaceAvailable {
    return CFWriteStreamCanAcceptBytes((CFWriteStreamRef)self);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSStreamBridgeInit(void) {
    NSStreamFileCurrentOffsetKey = (NSStreamPropertyKey)kCFStreamPropertyFileCurrentOffset;
    _CFRuntimeBridgeClasses(CFReadStreamGetTypeID(), "NSInputStream");
    _CFRuntimeBridgeClasses(CFWriteStreamGetTypeID(), "NSOutputStream");
}
#endif
