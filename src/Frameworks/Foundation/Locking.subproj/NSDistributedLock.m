/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSDistributedLock.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDate.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* The lock is a directory: mkdir is atomic across processes on every file
 * system we care about, which is the property NSDistributedLock needs. */

@implementation NSDistributedLock

+ (NSDistributedLock *)lockWithPath:(NSString *)path {
    return [[[self alloc] initWithPath:path] autorelease];
}

- (instancetype)initWithPath:(NSString *)path {
    self = [super init];
    if (self != nil) {
        _path = [path copy];
    }
    return self;
}

- (void)dealloc {
    [_path release];
    [super dealloc];
}

- (BOOL)tryLock {
    if (mkdir([_path fileSystemRepresentation], 0755) == 0) {
        _locked = YES;
        return YES;
    }
    return NO;
}

- (void)unlock {
    if (_locked) {
        rmdir([_path fileSystemRepresentation]);
        _locked = NO;
    }
}

- (void)breakLock {
    rmdir([_path fileSystemRepresentation]);
    _locked = NO;
}

- (NSDate *)lockDate {
    struct stat info;

    if (stat([_path fileSystemRepresentation], &info) != 0) {
        return nil;
    }
    return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)info.st_mtime];
}

@end
