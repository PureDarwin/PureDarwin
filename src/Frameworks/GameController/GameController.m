// GameController.m - game controllers on a system with no controller service
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// no controller ever connects, so toolkits such as SDL fall back to reading HID devices through IOKit

#import "GameController.h"
#include <math.h>

NSString * const GCControllerDidConnectNotification = @"GCControllerDidConnectNotification";
NSString * const GCControllerDidDisconnectNotification = @"GCControllerDidDisconnectNotification";
NSString * const GCControllerDidBecomeCurrentNotification = @"GCControllerDidBecomeCurrentNotification";
NSString * const GCControllerDidStopBeingCurrentNotification = @"GCControllerDidStopBeingCurrentNotification";

NSString * const GCInputButtonA = @"Button A";
NSString * const GCInputButtonB = @"Button B";
NSString * const GCInputButtonX = @"Button X";
NSString * const GCInputButtonY = @"Button Y";
NSString * const GCInputButtonMenu = @"Button Menu";
NSString * const GCInputButtonOptions = @"Button Options";
NSString * const GCInputButtonHome = @"Button Home";
NSString * const GCInputButtonShare = @"Button Share";
NSString * const GCInputDirectionPad = @"Direction Pad";
NSString * const GCInputLeftShoulder = @"Left Shoulder";
NSString * const GCInputRightShoulder = @"Right Shoulder";
NSString * const GCInputLeftTrigger = @"Left Trigger";
NSString * const GCInputRightTrigger = @"Right Trigger";
NSString * const GCInputLeftThumbstick = @"Left Thumbstick";
NSString * const GCInputRightThumbstick = @"Right Thumbstick";
NSString * const GCInputLeftThumbstickButton = @"Left Thumbstick Button";
NSString * const GCInputRightThumbstickButton = @"Right Thumbstick Button";
NSString * const GCInputDualShockTouchpadOne = @"Touchpad 1";
NSString * const GCInputDualShockTouchpadTwo = @"Touchpad 2";
NSString * const GCInputDualShockTouchpadButton = @"Touchpad Button";
NSString * const GCInputXboxPaddleOne = @"Paddle 1";
NSString * const GCInputXboxPaddleTwo = @"Paddle 2";
NSString * const GCInputXboxPaddleThree = @"Paddle 3";
NSString * const GCInputXboxPaddleFour = @"Paddle 4";

NSString * const GCHapticsLocalityDefault = @"Default";
NSString * const GCHapticsLocalityAll = @"All";
NSString * const GCHapticsLocalityHandles = @"Handles";
NSString * const GCHapticsLocalityLeftHandle = @"Left Handle";
NSString * const GCHapticsLocalityRightHandle = @"Right Handle";
NSString * const GCHapticsLocalityTriggers = @"Triggers";
NSString * const GCHapticsLocalityLeftTrigger = @"Left Trigger";
NSString * const GCHapticsLocalityRightTrigger = @"Right Trigger";
const float GCHapticDurationInfinite = INFINITY;

@implementation GCControllerElement
- (BOOL)isAnalog { return NO; }
- (NSString *)localizedName { return nil; }
- (NSString *)sfSymbolsName { return nil; }
@end

@implementation GCControllerButtonInput
- (float)value { return 0; }
- (BOOL)isPressed { return NO; }
- (BOOL)isTouched { return NO; }
@end

@implementation GCControllerAxisInput
- (float)value { return 0; }
@end

@implementation GCColor {
    float _red, _green, _blue;
}

- (instancetype)initWithRed:(float)red green:(float)green blue:(float)blue
{
    if ((self = [super init]) != nil) {
        _red = red;
        _green = green;
        _blue = blue;
    }
    return self;
}

- (float)red { return _red; }
- (float)green { return _green; }
- (float)blue { return _blue; }
@end

@implementation GCController
@synthesize playerIndex = _playerIndex;

+ (NSArray<GCController *> *)controllers { return @[]; }
+ (GCController *)current { return nil; }
// HID game pads stay with IOKit, where SDL's own HID driver reads them
+ (BOOL)supportsHIDDevice:(void *)device { return NO; }

// discovery finds nothing and finishes straight away, on the main queue as GameController's does
+ (void)startWirelessControllerDiscoveryWithCompletionHandler:(void (^)(void))completionHandler
{
    if (completionHandler != nil)
        dispatch_async(dispatch_get_main_queue(), completionHandler);
}

+ (void)stopWirelessControllerDiscovery {}
- (NSString *)vendorName { return nil; }
- (NSString *)productCategory { return @"Unknown"; }
@end
