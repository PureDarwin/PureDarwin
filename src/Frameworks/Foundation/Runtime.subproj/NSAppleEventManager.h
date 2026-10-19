// NSAppleEventManager.h - the event handler table for Apple events delivered to this process
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <Foundation/NSObject.h>
#import <Foundation/NSAppleEventDescriptor.h>
#import <Foundation/NSNotification.h>

@class NSMutableDictionary;

FOUNDATION_EXPORT NSNotificationName const NSAppleEventManagerWillProcessFirstEventNotification;

typedef const struct __NSAppleEventManagerSuspension *NSAppleEventManagerSuspensionID;

@interface NSAppleEventManager : NSObject {
    NSMutableDictionary *_handlers;
    NSAppleEventDescriptor *_currentEvent;
    NSAppleEventDescriptor *_currentReply;
}

+ (NSAppleEventManager *)sharedAppleEventManager;

- (void)setEventHandler:(id)handler andSelector:(SEL)handleEventSelector forEventClass:(AEEventClass)eventClass
             andEventID:(AEEventID)eventID;
- (void)removeEventHandlerForEventClass:(AEEventClass)eventClass andEventID:(AEEventID)eventID;

// hands an event to its registered handler, the event is current while the handler runs
- (BOOL)dispatchAppleEvent:(NSAppleEventDescriptor *)event withReply:(NSAppleEventDescriptor *)reply;

@property (readonly, retain) NSAppleEventDescriptor *currentAppleEvent;
@property (readonly, retain) NSAppleEventDescriptor *currentReplyAppleEvent;

- (NSAppleEventManagerSuspensionID)suspendCurrentAppleEvent;
- (void)resumeWithSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID;

@end
