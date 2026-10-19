// NSAppleEventManager.m - the event handler table for Apple events delivered to this process
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// PD has no Apple event transport between processes, only events the process dispatches itself arrive
#import <Foundation/NSAppleEventManager.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNotification.h>
#include <objc/message.h>

NSNotificationName const NSAppleEventManagerWillProcessFirstEventNotification = @"NSAppleEventManagerWillProcessFirstEventNotification";

@interface NSAppleEventHandlerEntry : NSObject {
@public
    id _handler;
    SEL _selector;
}
@end

@implementation NSAppleEventHandlerEntry
@end

static NSNumber *handlerKey(AEEventClass eventClass, AEEventID eventID) {
    return [NSNumber numberWithUnsignedLongLong:((unsigned long long)eventClass << 32) | eventID];
}

@implementation NSAppleEventManager

+ (NSAppleEventManager *)sharedAppleEventManager {
    static NSAppleEventManager *shared;

    @synchronized(self) {
        if (shared == nil) shared = [[NSAppleEventManager alloc] init];
    }
    return shared;
}

- (instancetype)init {
    self = [super init];
    if (self == nil) return nil;
    _handlers = [[NSMutableDictionary alloc] init];
    return self;
}

- (void)dealloc {
    [_handlers release];
    [_currentEvent release];
    [_currentReply release];
    [super dealloc];
}

- (void)setEventHandler:(id)handler andSelector:(SEL)handleEventSelector forEventClass:(AEEventClass)eventClass
             andEventID:(AEEventID)eventID {
    NSAppleEventHandlerEntry *entry = [[NSAppleEventHandlerEntry alloc] init];

    // the handler is not retained, as on macOS
    entry->_handler = handler;
    entry->_selector = handleEventSelector;
    @synchronized(self) {
        [_handlers setObject:entry forKey:handlerKey(eventClass, eventID)];
    }
    [entry release];
}

- (void)removeEventHandlerForEventClass:(AEEventClass)eventClass andEventID:(AEEventID)eventID {
    @synchronized(self) {
        [_handlers removeObjectForKey:handlerKey(eventClass, eventID)];
    }
}

- (BOOL)dispatchAppleEvent:(NSAppleEventDescriptor *)event withReply:(NSAppleEventDescriptor *)reply {
    NSAppleEventHandlerEntry *entry;

    @synchronized(self) {
        entry = [[_handlers objectForKey:handlerKey([event eventClass], [event eventID])] retain];
    }
    if (entry == nil) return NO;

    NSAppleEventDescriptor *savedEvent = _currentEvent, *savedReply = _currentReply;

    _currentEvent = [event retain];
    _currentReply = [reply retain];
    ((void (*)(id, SEL, id, id))objc_msgSend)(entry->_handler, entry->_selector, event, reply);
    [_currentEvent release];
    [_currentReply release];
    _currentEvent = savedEvent;
    _currentReply = savedReply;
    [entry release];
    return YES;
}

- (NSAppleEventDescriptor *)currentAppleEvent {
    return _currentEvent;
}

- (NSAppleEventDescriptor *)currentReplyAppleEvent {
    return _currentReply;
}

- (NSAppleEventManagerSuspensionID)suspendCurrentAppleEvent {
    return (NSAppleEventManagerSuspensionID)_currentEvent;
}

- (void)resumeWithSuspensionID:(NSAppleEventManagerSuspensionID)suspensionID {
}

@end
