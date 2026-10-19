// NSHapticFeedback.m - trackpad haptic feedback, with no haptic device to drive
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSHapticFeedback.h>

// no Force Touch trackpad driver exists, so feedback has nowhere to go, as on a Mac with a plain trackpad
@interface PDHapticFeedbackPerformer : NSObject <NSHapticFeedbackPerformer>
@end

@implementation PDHapticFeedbackPerformer

- (void)performFeedbackPattern:(NSHapticFeedbackPattern)pattern performanceTime:(NSHapticFeedbackPerformanceTime)performanceTime
{
}

@end

@implementation NSHapticFeedbackManager

+ (id<NSHapticFeedbackPerformer>)defaultPerformer
{
    static PDHapticFeedbackPerformer *performer;

    if (performer == nil)
        performer = [[PDHapticFeedbackPerformer alloc] init];
    return performer;
}

@end
