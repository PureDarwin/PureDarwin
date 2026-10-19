// NSHapticFeedback.h - trackpad haptic feedback, with no haptic device to drive
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>

typedef NS_ENUM(NSInteger, NSHapticFeedbackPattern) {
    NSHapticFeedbackPatternGeneric = 0,
    NSHapticFeedbackPatternAlignment,
    NSHapticFeedbackPatternLevelChange,
};

typedef NS_ENUM(NSUInteger, NSHapticFeedbackPerformanceTime) {
    NSHapticFeedbackPerformanceTimeDefault = 0,
    NSHapticFeedbackPerformanceTimeNow,
    NSHapticFeedbackPerformanceTimeDrawCompleted,
};

@protocol NSHapticFeedbackPerformer <NSObject>
- (void)performFeedbackPattern:(NSHapticFeedbackPattern)pattern performanceTime:(NSHapticFeedbackPerformanceTime)performanceTime;
@end

@interface NSHapticFeedbackManager : NSObject
+ (id<NSHapticFeedbackPerformer>)defaultPerformer;
@end
