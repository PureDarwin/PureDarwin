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
#import <Foundation/NSNotification.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSValue.h>
#include <CoreFoundation/CFRunLoop.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

@implementation NSFileHandle {
    int _fd;
    BOOL _closeOnDealloc;
    CFRunLoopRef _waitRunLoop;
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

NSString *const NSFileHandleDataAvailableNotification =
    @"NSFileHandleDataAvailableNotification";

/*
 * Apple delivers this on the run loop that registered the wait, so that is the
 * one captured here and woken when the descriptor becomes readable. The wait
 * itself is a blocking select on a detached thread, and there is no run-loop
 * source plumbing for descriptors yet.
 */
- (void)__waitForDataThread:(id)ignored {
    int fd = _fd;
    fd_set readSet;

    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);

    /* A failed select still notifies: the observer's read reports the error,
     * which is more useful than never being told. */
    (void)select(fd + 1, &readSet, NULL, NULL, NULL);

    CFRunLoopRef runLoop = _waitRunLoop;

    if (runLoop == NULL) {
        return;
    }

    NSFileHandle *handle = [self retain];

    CFRunLoopPerformBlock(runLoop, kCFRunLoopCommonModes, ^{
        [[NSNotificationCenter defaultCenter]
            postNotificationName:NSFileHandleDataAvailableNotification
                          object:handle];
        [handle release];
    });
    CFRunLoopWakeUp(runLoop);
}

- (void)waitForDataInBackgroundAndNotify {
    _waitRunLoop = CFRunLoopGetCurrent();

    [NSThread detachNewThreadSelector:@selector(__waitForDataThread:)
                             toTarget:self
                           withObject:nil];
}

- (void)closeFile {
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}


/* Returns whatever can be read without blocking further; for a regular file
 * that is the rest of the file. */
- (NSData *)availableData {
    return [self readDataToEndOfFile];
}

@end
