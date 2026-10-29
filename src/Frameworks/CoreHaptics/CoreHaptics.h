// CoreHaptics.h - haptic patterns and the engine that plays them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString * const CHHapticEventTypeHapticTransient;
FOUNDATION_EXPORT NSString * const CHHapticEventTypeHapticContinuous;
FOUNDATION_EXPORT NSString * const CHHapticEventParameterIDHapticIntensity;
FOUNDATION_EXPORT NSString * const CHHapticEventParameterIDHapticSharpness;
FOUNDATION_EXPORT NSString * const CHHapticDynamicParameterIDHapticIntensityControl;
FOUNDATION_EXPORT NSString * const CHHapticDynamicParameterIDHapticSharpnessControl;
FOUNDATION_EXPORT NSString * const CHHapticErrorDomain;

@interface CHHapticEventParameter : NSObject
- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value;
@property (nonatomic, readonly) NSString *parameterID;
@property (nonatomic) float value;
@end

@interface CHHapticDynamicParameter : NSObject
- (instancetype)initWithParameterID:(NSString *)parameterID value:(float)value relativeTime:(NSTimeInterval)time;
@property (nonatomic, readonly) NSString *parameterID;
@property (nonatomic) float value;
@property (nonatomic) NSTimeInterval relativeTime;
@end

@interface CHHapticEvent : NSObject
- (instancetype)initWithEventType:(NSString *)type parameters:(NSArray<CHHapticEventParameter *> *)eventParams
                     relativeTime:(NSTimeInterval)time duration:(NSTimeInterval)duration;
@property (nonatomic, readonly) NSString *type;
@property (nonatomic, readonly) NSArray<CHHapticEventParameter *> *eventParameters;
@property (nonatomic) NSTimeInterval relativeTime;
@property (nonatomic) NSTimeInterval duration;
@end

@interface CHHapticPattern : NSObject
- (instancetype)initWithEvents:(NSArray<CHHapticEvent *> *)events parameters:(NSArray<CHHapticDynamicParameter *> *)parameters
                         error:(NSError **)outError;
@property (nonatomic, readonly) NSTimeInterval duration;
@end
