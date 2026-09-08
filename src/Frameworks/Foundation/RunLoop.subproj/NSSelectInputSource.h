/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSSelectInputSource_h
#define NSSelectInputSource_h

#import <Foundation/NSInputSource.h>
#import <Foundation/NSObjCRuntime.h>

@class NSSocket, NSSelectInputSource;

typedef NS_OPTIONS(NSUInteger, NSSelectEventMask) {
    NSSelectReadEvent   = 0x01,
    NSSelectWriteEvent  = 0x02,
    NSSelectExceptEvent = 0x04,
};

@protocol NSSelectInputSourceDelegate <NSObject>
@optional
- (void)selectInputSource:(NSSelectInputSource *)inputSource
            selectEvent:(NSSelectEventMask)selectEvent;
@end

@interface NSSelectInputSource : NSInputSource {
    NSSocket *_socket;
    NSSelectEventMask _selectEventMask;
    id _delegate;
}

+ (instancetype)socketInputSourceWithSocket:(NSSocket *)socket;

- (instancetype)initWithSocket:(NSSocket *)socket;

- (NSSocket *)socket;
- (int)fileDescriptor;

- (NSSelectEventMask)selectEventMask;
- (void)setSelectEventMask:(NSSelectEventMask)mask;

- (id)delegate;
- (void)setDelegate:(id)delegate;

- (void)processImmediateEvents:(NSSelectEventMask)selectEvent;

@end

#endif /* NSSelectInputSource_h */
