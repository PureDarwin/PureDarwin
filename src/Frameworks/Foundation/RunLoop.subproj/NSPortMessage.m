// NSPortMessage.m - a message for an NSPort
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSPortMessage.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSPort.h>

@implementation NSPortMessage

- (instancetype)initWithSendPort:(NSPort *)sendPort receivePort:(NSPort *)replyPort components:(NSArray *)components {
    if ((self = [super init]) == nil)
        return nil;
    _sendPort = [sendPort retain];
    _receivePort = [replyPort retain];
    _components = [components retain];
    return self;
}

- (void)dealloc {
    [_sendPort release];
    [_receivePort release];
    [_components release];
    [super dealloc];
}

- (NSArray *)components {
    return _components;
}

- (NSPort *)receivePort {
    return _receivePort;
}

- (NSPort *)sendPort {
    return _sendPort;
}

- (uint32_t)msgid {
    return _msgid;
}

- (void)setMsgid:(uint32_t)msgid {
    _msgid = msgid;
}

- (BOOL)sendBeforeDate:(NSDate *)date {
    NSMutableArray *components = [[_components mutableCopy] autorelease];

    return [_sendPort sendBeforeDate:date msgid:_msgid components:components from:_receivePort reserved:0];
}

@end
