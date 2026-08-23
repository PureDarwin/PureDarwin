/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSFileHandle.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <fcntl.h>
#include <unistd.h>

@implementation NSFileHandle {
    int _fd;
    BOOL _closeOnDealloc;
}

static NSFileHandle *_open(Class cls, NSString *path, int flags) {
    char buffer[PATH_MAX];
    if (!CFStringGetCString((CFStringRef)path, buffer, sizeof(buffer),
                            kCFStringEncodingUTF8)) {
        return nil;
    }

    int fd = open(buffer, flags);
    if (fd < 0) {
        return nil;
    }
    return [[cls alloc] initWithFileDescriptor:fd closeOnDealloc:YES];
}

+ (NSFileHandle *)fileHandleForReadingAtPath:(NSString *)path {
    return _open(self, path, O_RDONLY);
}

+ (NSFileHandle *)fileHandleForWritingAtPath:(NSString *)path {
    return _open(self, path, O_WRONLY);
}

+ (NSFileHandle *)fileHandleForUpdatingAtPath:(NSString *)path {
    return _open(self, path, O_RDWR);
}

+ (NSFileHandle *)fileHandleWithStandardInput {
    return [[self alloc] initWithFileDescriptor:STDIN_FILENO];
}

+ (NSFileHandle *)fileHandleWithStandardOutput {
    return [[self alloc] initWithFileDescriptor:STDOUT_FILENO];
}

+ (NSFileHandle *)fileHandleWithStandardError {
    return [[self alloc] initWithFileDescriptor:STDERR_FILENO];
}

+ (NSFileHandle *)fileHandleWithNullDevice {
    int fd = open("/dev/null", O_RDWR);
    return fd < 0 ? nil : [[self alloc] initWithFileDescriptor:fd closeOnDealloc:YES];
}

- (instancetype)initWithFileDescriptor:(int)fd {
    return [self initWithFileDescriptor:fd closeOnDealloc:NO];
}

- (instancetype)initWithFileDescriptor:(int)fd closeOnDealloc:(BOOL)closeOnDealloc {
    self = [super init];
    if (self != nil) {
        _fd = fd;
        _closeOnDealloc = closeOnDealloc;
    }
    return self;
}

- (void)dealloc {
    if (_closeOnDealloc && _fd >= 0) {
        close(_fd);
    }
}

- (int)fileDescriptor {
    return _fd;
}

- (NSData *)readDataToEndOfFile {
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    unsigned char chunk[65536];
    ssize_t got;

    while ((got = read(_fd, chunk, sizeof(chunk))) > 0) {
        CFDataAppendBytes(data, chunk, (CFIndex)got);
    }
    return (NSData *)data;
}

- (NSData *)readDataOfLength:(NSUInteger)length {
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    unsigned char chunk[65536];
    NSUInteger remaining = length;

    while (remaining > 0) {
        size_t want = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        ssize_t got = read(_fd, chunk, want);
        if (got <= 0) {
            break;
        }
        CFDataAppendBytes(data, chunk, (CFIndex)got);
        remaining -= (NSUInteger)got;
    }
    return (NSData *)data;
}

- (void)writeData:(NSData *)data {
    const unsigned char *bytes = [data bytes];
    NSUInteger remaining = [data length];

    while (remaining > 0) {
        ssize_t wrote = write(_fd, bytes, remaining);
        if (wrote <= 0) {
            break;
        }
        bytes += wrote;
        remaining -= (NSUInteger)wrote;
    }
}

- (unsigned long long)offsetInFile {
    return (unsigned long long)lseek(_fd, 0, SEEK_CUR);
}

- (unsigned long long)seekToEndOfFile {
    return (unsigned long long)lseek(_fd, 0, SEEK_END);
}

- (void)seekToFileOffset:(unsigned long long)offset {
    lseek(_fd, (off_t)offset, SEEK_SET);
}

- (void)truncateFileAtOffset:(unsigned long long)offset {
    ftruncate(_fd, (off_t)offset);
    lseek(_fd, (off_t)offset, SEEK_SET);
}

- (void)synchronizeFile {
    fsync(_fd);
}

- (void)closeFile {
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}

@end
