// NSPortMessage.h - a message for an NSPort
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSObject.h>

@class NSArray, NSDate, NSPort;

// a message for an NSPort: an id and components, NSData and NSPort objects
@interface NSPortMessage : NSObject {
    NSPort *_sendPort;
    NSPort *_receivePort;
    NSArray *_components;
    uint32_t _msgid;
}

- (instancetype)initWithSendPort:(NSPort *)sendPort receivePort:(NSPort *)replyPort components:(NSArray *)components;

@property (readonly, retain) NSArray *components;
@property (readonly, retain) NSPort *receivePort;
@property (readonly, retain) NSPort *sendPort;
- (BOOL)sendBeforeDate:(NSDate *)date;
@property uint32_t msgid;

@end
