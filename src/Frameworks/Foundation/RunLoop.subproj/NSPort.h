/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPort_h
#define NSPort_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRunLoop.h>
#include <mach/mach.h>

@class NSRunLoop, NSDate, NSMutableArray, NSPortMessage;

FOUNDATION_EXPORT NSString * const NSPortDidBecomeInvalidNotification;

/* Backed by a real Mach port; NSMachPort is the only concrete kind here. */
@protocol NSPortDelegate <NSObject>
@optional
- (void)handlePortMessage:(NSPortMessage *)message;
@end

@protocol NSMachPortDelegate <NSPortDelegate>
@optional
- (void)handleMachMessage:(void *)msg;
@end

@interface NSPort : NSObject {
    mach_port_t _machPort;
    BOOL _ownsPort;
    BOOL _valid;
    id _delegate;
    // the CFMachPort and its run loop source once the port is scheduled
    void *_cfPort;
    void *_cfSource;
}

+ (NSPort *)port;

- (void)invalidate;
- (BOOL)isValid;

- (void)setDelegate:(id)delegate;
- (id)delegate;

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode;
- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode;

- (NSUInteger)reservedSpaceLength;
- (BOOL)sendBeforeDate:(NSDate *)limitDate components:(NSMutableArray *)components
                  from:(NSPort *)receivePort reserved:(NSUInteger)headerSpaceReserved;
- (BOOL)sendBeforeDate:(NSDate *)limitDate msgid:(NSUInteger)msgID components:(NSMutableArray *)components
                  from:(NSPort *)receivePort reserved:(NSUInteger)headerSpaceReserved;

@end

@interface NSMachPort : NSPort

+ (NSPort *)portWithMachPort:(uint32_t)machPort;
+ (NSPort *)portWithMachPort:(uint32_t)machPort options:(NSUInteger)options;

- (instancetype)initWithMachPort:(uint32_t)machPort;

- (uint32_t)machPort;

@end

#endif /* NSPort_h */
