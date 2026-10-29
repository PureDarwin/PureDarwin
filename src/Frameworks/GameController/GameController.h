// GameController.h - game controllers, the classes SDL and other toolkits look up
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString * const GCControllerDidConnectNotification;
FOUNDATION_EXPORT NSString * const GCControllerDidDisconnectNotification;
FOUNDATION_EXPORT NSString * const GCControllerDidBecomeCurrentNotification;
FOUNDATION_EXPORT NSString * const GCControllerDidStopBeingCurrentNotification;

// the names the physical input profile files its elements under
FOUNDATION_EXPORT NSString * const GCInputButtonA;
FOUNDATION_EXPORT NSString * const GCInputButtonB;
FOUNDATION_EXPORT NSString * const GCInputButtonX;
FOUNDATION_EXPORT NSString * const GCInputButtonY;
FOUNDATION_EXPORT NSString * const GCInputButtonMenu;
FOUNDATION_EXPORT NSString * const GCInputButtonOptions;
FOUNDATION_EXPORT NSString * const GCInputButtonHome;
FOUNDATION_EXPORT NSString * const GCInputButtonShare;
FOUNDATION_EXPORT NSString * const GCInputDirectionPad;
FOUNDATION_EXPORT NSString * const GCInputLeftShoulder;
FOUNDATION_EXPORT NSString * const GCInputRightShoulder;
FOUNDATION_EXPORT NSString * const GCInputLeftTrigger;
FOUNDATION_EXPORT NSString * const GCInputRightTrigger;
FOUNDATION_EXPORT NSString * const GCInputLeftThumbstick;
FOUNDATION_EXPORT NSString * const GCInputRightThumbstick;
FOUNDATION_EXPORT NSString * const GCInputLeftThumbstickButton;
FOUNDATION_EXPORT NSString * const GCInputRightThumbstickButton;
FOUNDATION_EXPORT NSString * const GCInputDualShockTouchpadOne;
FOUNDATION_EXPORT NSString * const GCInputDualShockTouchpadTwo;
FOUNDATION_EXPORT NSString * const GCInputDualShockTouchpadButton;
FOUNDATION_EXPORT NSString * const GCInputXboxPaddleOne;
FOUNDATION_EXPORT NSString * const GCInputXboxPaddleTwo;
FOUNDATION_EXPORT NSString * const GCInputXboxPaddleThree;
FOUNDATION_EXPORT NSString * const GCInputXboxPaddleFour;

// where a controller's haptics sit
FOUNDATION_EXPORT NSString * const GCHapticsLocalityDefault;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityAll;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityHandles;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityLeftHandle;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityRightHandle;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityTriggers;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityLeftTrigger;
FOUNDATION_EXPORT NSString * const GCHapticsLocalityRightTrigger;
FOUNDATION_EXPORT const float GCHapticDurationInfinite;

@interface GCControllerElement : NSObject
@property (nonatomic, readonly) BOOL isAnalog;
@property (nonatomic, readonly, copy) NSString *localizedName;
@property (nonatomic, readonly, copy) NSString *sfSymbolsName;
@end

@interface GCControllerButtonInput : GCControllerElement
@property (nonatomic, readonly) float value;
@property (nonatomic, readonly, getter=isPressed) BOOL pressed;
@property (nonatomic, readonly, getter=isTouched) BOOL touched;
@end

@interface GCControllerAxisInput : GCControllerElement
@property (nonatomic, readonly) float value;
@end

@interface GCColor : NSObject
- (instancetype)initWithRed:(float)red green:(float)green blue:(float)blue;
@property (nonatomic, readonly) float red;
@property (nonatomic, readonly) float green;
@property (nonatomic, readonly) float blue;
@end

@interface GCController : NSObject
+ (NSArray<GCController *> *)controllers;
+ (GCController *)current;
+ (BOOL)supportsHIDDevice:(void *)device;
+ (void)startWirelessControllerDiscoveryWithCompletionHandler:(void (^)(void))completionHandler;
+ (void)stopWirelessControllerDiscovery;
@property (nonatomic, readonly, copy) NSString *vendorName;
@property (nonatomic, readonly, copy) NSString *productCategory;
@property (nonatomic) NSInteger playerIndex;
@end
