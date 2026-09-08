/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSSelectInputSource.h>
#import <Foundation/NSSocket.h>

@implementation NSInputSource

- (void)fire {
}

@end

@implementation NSSelectInputSource

+ (instancetype)socketInputSourceWithSocket:(NSSocket *)socket {
    return [[[self alloc] initWithSocket:socket] autorelease];
}

- (instancetype)initWithSocket:(NSSocket *)socket {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _socket = [socket retain];
    _selectEventMask = NSSelectReadEvent;
    return self;
}

- (void)dealloc {
    [_socket release];
    [super dealloc];
}

- (NSSocket *)socket {
    return _socket;
}

- (int)fileDescriptor {
    return [_socket fileDescriptor];
}

- (NSSelectEventMask)selectEventMask {
    return _selectEventMask;
}

- (void)setSelectEventMask:(NSSelectEventMask)mask {
    _selectEventMask = mask;
}

/* The delegate is not retained: it is normally the object that owns the
 * source. */
- (id)delegate {
    return _delegate;
}

- (void)setDelegate:(id)delegate {
    _delegate = delegate;
}

- (void)processImmediateEvents:(NSSelectEventMask)selectEvent {
    if ([_delegate respondsToSelector:@selector(selectInputSource:selectEvent:)]) {
        [_delegate selectInputSource:self selectEvent:selectEvent];
    }
}

- (void)fire {
    [self processImmediateEvents:_selectEventMask];
}

@end
