/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNotificationQueue.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFRunLoop.h>

// the NSPostNow style posts at once, ASAP and WhenIdle wait for the run loop of the thread that queued them:
// an observer posts ASAP ones on its next pass, idle ones when it is about to wait or a pass ends

@interface NSNotificationQueue ()
- (void)_postQueued:(BOOL)idle;
@end

static void notificationQueueObserver(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
    BOOL idle = (activity & (kCFRunLoopBeforeWaiting | kCFRunLoopExit)) != 0;

    [(NSNotificationQueue *)info _postQueued:idle];
}

@implementation NSNotificationQueue

+ (NSNotificationQueue *)defaultQueue {
    static NSNotificationQueue *shared = nil;

    if (shared == nil) {
        shared = [[self alloc] initWithNotificationCenter:
                    [NSNotificationCenter defaultCenter]];
    }
    return shared;
}

- (instancetype)initWithNotificationCenter:(NSNotificationCenter *)center {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _center = [center retain];
    _asapQueue = [[NSMutableArray alloc] init];
    _idleQueue = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc {
    if (_observer != NULL) {
        CFRunLoopObserverInvalidate((CFRunLoopObserverRef)_observer);
        CFRelease((CFRunLoopObserverRef)_observer);
    }
    [_center release];
    [_asapQueue release];
    [_idleQueue release];
    [super dealloc];
}

- (void)enqueueNotification:(NSNotification *)notification
               postingStyle:(NSPostingStyle)style {
    [self enqueueNotification:notification
                 postingStyle:style
                 coalesceMask:NSNotificationCoalescingOnName
                     forModes:nil];
}

- (void)enqueueNotification:(NSNotification *)notification
               postingStyle:(NSPostingStyle)style
               coalesceMask:(NSNotificationCoalescing)coalesceMask
                   forModes:(NSArray *)modes {
    if (style == NSPostNow) {
        [_center postNotification:notification];
        return;
    }

    if (coalesceMask != NSNotificationNoCoalescing) {
        [self dequeueNotificationsMatching:notification coalesceMask:coalesceMask];
    }
    [(style == NSPostASAP) ? _asapQueue : _idleQueue addObject:notification];

    // the observer goes on the queuing thread's run loop the first time, a wake-up gets it a pass soon
    if (_observer == NULL) {
        CFRunLoopObserverContext context = { 0, self, NULL, NULL, NULL };
        CFRunLoopObserverRef observer = CFRunLoopObserverCreate(kCFAllocatorDefault,
            kCFRunLoopBeforeTimers | kCFRunLoopBeforeSources | kCFRunLoopBeforeWaiting | kCFRunLoopExit,
            YES, 0, notificationQueueObserver, &context);

        CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer, kCFRunLoopCommonModes);
        _observer = observer;
    }
    CFRunLoopWakeUp(CFRunLoopGetCurrent());
}

// posted from a copy, so observers can queue more for a later pass
- (void)_postQueued:(BOOL)idle {
    NSMutableArray *queues[2] = { _asapQueue, idle ? _idleQueue : nil };

    for (int q = 0; q < 2; q++) {
        if ([queues[q] count] == 0)
            continue;

        NSArray *pending = [queues[q] copy];

        [queues[q] removeAllObjects];
        for (NSNotification *notification in pending)
            [_center postNotification:notification];
        [pending release];
    }
}

- (void)dequeueNotificationsMatching:(NSNotification *)notification
                        coalesceMask:(NSUInteger)coalesceMask {
    NSMutableArray *queues[2] = { _asapQueue, _idleQueue };

    for (int q = 0; q < 2; q++) {
        NSUInteger i = [queues[q] count];

        while (i-- > 0) {
            NSNotification *queued = [queues[q] objectAtIndex:i];
            BOOL matches = YES;

            if (coalesceMask & NSNotificationCoalescingOnName) {
                matches = matches && [[queued name] isEqualToString:[notification name]];
            }
            if (coalesceMask & NSNotificationCoalescingOnSender) {
                matches = matches && ([queued object] == [notification object]);
            }
            if (matches) {
                [queues[q] removeObjectAtIndex:i];
            }
        }
    }
}

@end
