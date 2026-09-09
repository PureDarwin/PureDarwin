/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPort.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>

NSString * const NSPortDidBecomeInvalidNotification = @"NSPortDidBecomeInvalidNotification";

@implementation NSPort

+ (NSPort *)port {
    return [[[NSMachPort alloc] init] autorelease];
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        mach_port_t port = MACH_PORT_NULL;

        if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                               &port) != KERN_SUCCESS) {
            [self release];
            return nil;
        }
        if (mach_port_insert_right(mach_task_self(), port, port,
                                   MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
            mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
            [self release];
            return nil;
        }
        _machPort = port;
        _ownsPort = YES;
        _valid = YES;
    }
    return self;
}

- (void)dealloc {
    [self invalidate];
    [super dealloc];
}

- (void)invalidate {
    if (!_valid) {
        return;
    }
    _valid = NO;

    if (_ownsPort && _machPort != MACH_PORT_NULL) {
        mach_port_mod_refs(mach_task_self(), _machPort, MACH_PORT_RIGHT_RECEIVE, -1);
        mach_port_deallocate(mach_task_self(), _machPort);
    }
    _machPort = MACH_PORT_NULL;

    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSPortDidBecomeInvalidNotification object:self];
}

- (BOOL)isValid {
    return _valid;
}

- (void)setDelegate:(id)delegate {
    _delegate = delegate;
}

- (id)delegate {
    return _delegate;
}

/* NSRunLoop here is select()-driven and has no Mach port source, so a port is
 * accepted but never signals the loop. -[NSConnection registerName:] does not
 * rely on this; it services its port on a dispatch queue. */
- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode {
}

- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode {
}

@end

@implementation NSMachPort

+ (NSPort *)portWithMachPort:(uint32_t)machPort {
    return [[[self alloc] initWithMachPort:machPort] autorelease];
}

+ (NSPort *)portWithMachPort:(uint32_t)machPort options:(NSUInteger)options {
    return [self portWithMachPort:machPort];
}

- (instancetype)initWithMachPort:(uint32_t)machPort {
    self = [super init];
    if (self != nil) {
        /* -[NSPort init] allocated one; adopt the caller's instead. */
        if (_ownsPort && _machPort != MACH_PORT_NULL) {
            mach_port_mod_refs(mach_task_self(), _machPort, MACH_PORT_RIGHT_RECEIVE, -1);
            mach_port_deallocate(mach_task_self(), _machPort);
        }
        _machPort = (mach_port_t)machPort;
        _ownsPort = NO;
        _valid = (machPort != MACH_PORT_NULL);
    }
    return self;
}

- (uint32_t)machPort {
    return (uint32_t)_machPort;
}

@end
