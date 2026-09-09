/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPipe.h>
#import <Foundation/NSFileHandle.h>
#include <unistd.h>

@implementation NSPipe

+ (instancetype)pipe {
    return [[[self alloc] init] autorelease];
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        int fds[2];

        if (pipe(fds) != 0) {
            [self release];
            return nil;
        }
        _readHandle = [[NSFileHandle alloc] initWithFileDescriptor:fds[0]
                                                    closeOnDealloc:YES];
        _writeHandle = [[NSFileHandle alloc] initWithFileDescriptor:fds[1]
                                                     closeOnDealloc:YES];
    }
    return self;
}

- (void)dealloc {
    [_readHandle release];
    [_writeHandle release];
    [super dealloc];
}

- (NSFileHandle *)fileHandleForReading {
    return _readHandle;
}

- (NSFileHandle *)fileHandleForWriting {
    return _writeHandle;
}

@end
