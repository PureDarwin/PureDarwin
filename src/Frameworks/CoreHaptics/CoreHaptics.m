// CoreHaptics.m - haptic patterns on a system with no haptic hardware
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// patterns are plain values, nothing creates an engine to play them, so callers keep haptics off

#import "CoreHaptics.h"

NSString * const CHHapticEventTypeHapticTransient = @"HapticTransient";
NSString * const CHHapticEventTypeHapticContinuous = @"HapticContinuous";
NSString * const CHHapticEventParameterIDHapticIntensity = @"HapticIntensity";
NSString * const CHHapticEventParameterIDHapticSharpness = @"HapticSharpness";
NSString * const CHHapticDynamicParameterIDHapticIntensityControl = @"HapticIntensityControl";
NSString * const CHHapticDynamicParameterIDHapticSharpnessControl = @"HapticSharpnessControl";
NSString * const CHHapticErrorDomain = @"com.apple.CoreHaptics";

@implementation CHHapticEventParameter
@synthesize parameterID = _parameterID, value = _value;

- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value
{
    if ((self = [super init]) != nil) {
        _parameterID = [parameterID copy];
        _value = value;
    }
    return self;
}

- (void)dealloc
{
    [_parameterID release];
    [super dealloc];
}
@end

@implementation CHHapticDynamicParameter
@synthesize parameterID = _parameterID, value = _value, relativeTime = _relativeTime;

- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value relativeTime:(NSTimeInterval)time
{
    if ((self = [super init]) != nil) {
        _parameterID = [parameterID copy];
        _value = value;
        _relativeTime = time;
    }
    return self;
}

- (void)dealloc
{
    [_parameterID release];
    [super dealloc];
}
@end

@implementation CHHapticEvent
@synthesize type = _type, eventParameters = _eventParameters, relativeTime = _relativeTime, duration = _duration;

- (instancetype)initWithEventType:(NSString *)type parameters:(NSArray<CHHapticEventParameter *> *)eventParams
                     relativeTime:(NSTimeInterval)time duration:(NSTimeInterval)duration
{
    if ((self = [super init]) != nil) {
        _type = [type copy];
        _eventParameters = [eventParams copy];
        _relativeTime = time;
        _duration = duration;
    }
    return self;
}

- (void)dealloc
{
    [_type release];
    [_eventParameters release];
    [super dealloc];
}
@end

@implementation CHHapticPattern {
    NSTimeInterval _duration;
}

- (instancetype)initWithEvents:(NSArray<CHHapticEvent *> *)events parameters:(NSArray<CHHapticDynamicParameter *> *)parameters
                         error:(NSError **)outError
{
    if ((self = [super init]) != nil) {
        for (CHHapticEvent *event in events)
            _duration = MAX(_duration, event.relativeTime + event.duration);
    }
    return self;
}

- (NSTimeInterval)duration { return _duration; }
@end
