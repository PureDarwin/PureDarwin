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

/* NSPostNow posts straight through. ASAP and WhenIdle are queued, but nothing
 * drains them yet - that wants a run-loop observer, and every caller in AppKit
 * today uses NSPostNow. */

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
