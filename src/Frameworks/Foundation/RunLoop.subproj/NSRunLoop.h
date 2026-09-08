/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSRunLoop_h
#define NSRunLoop_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSDate, NSString, NSTimer, NSMutableArray, NSMutableDictionary, NSInputSource;

typedef NSString *NSRunLoopMode;

FOUNDATION_EXPORT NSRunLoopMode const NSDefaultRunLoopMode;
FOUNDATION_EXPORT NSRunLoopMode const NSRunLoopCommonModes;
FOUNDATION_EXPORT NSRunLoopMode const NSEventTrackingRunLoopMode;
FOUNDATION_EXPORT NSRunLoopMode const NSModalPanelRunLoopMode;

@interface NSRunLoop : NSObject {
    NSMutableDictionary *_modeToSources;
    NSMutableArray *_timers;
    NSMutableArray *_performs;
    NSString *_currentMode;
}

+ (NSRunLoop *)currentRunLoop;
+ (NSRunLoop *)mainRunLoop;

- (NSString *)currentMode;

- (void)addInputSource:(NSInputSource *)source forMode:(NSRunLoopMode)mode;
- (void)removeInputSource:(NSInputSource *)source forMode:(NSRunLoopMode)mode;

- (void)addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode;

- (void)performSelector:(SEL)selector target:(id)target argument:(id)argument
                  order:(NSUInteger)order modes:(NSArray *)modes;
- (void)cancelPerformSelector:(SEL)selector target:(id)target argument:(id)argument;

- (void)run;
- (void)runUntilDate:(NSDate *)date;
- (BOOL)runMode:(NSRunLoopMode)mode beforeDate:(NSDate *)date;
- (NSDate *)limitDateForMode:(NSRunLoopMode)mode;

@end

#endif /* NSRunLoop_h */
