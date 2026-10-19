// NSEvent_Modern.m - the phase, pressure and CGEvent API added to NSEvent after the original set
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSEvent.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

@implementation NSEvent (ModernEvents)

// the input stack delivers discrete wheel steps without gesture phases, which macOS reports as none
- (NSEventPhase)phase
{
    return NSEventPhaseNone;
}

- (NSEventPhase)momentumPhase
{
    return NSEventPhaseNone;
}

// a click with a plain mouse is stage one while the button is down, as macOS reports it
- (NSInteger)stage
{
    switch ([self type]) {
    case NSLeftMouseDown:
    case NSLeftMouseDragged:
    case NSRightMouseDown:
    case NSRightMouseDragged:
    case NSOtherMouseDown:
    case NSOtherMouseDragged:
        return 1;
    default:
        return 0;
    }
}

- (CGFloat)stageTransition
{
    return 0;
}

- (float)tangentialPressure
{
    return 0;
}

- (NSPoint)tilt
{
    return NSZeroPoint;
}

- (float)rotation
{
    return 0;
}

- (CGFloat)magnification
{
    return 0;
}

- (BOOL)isEnteringProximity
{
    return NO;
}

- (BOOL)isDirectionInvertedFromDevice
{
    return NO;
}

+ (BOOL)isSwipeTrackingFromScrollEventsEnabled
{
    return NO;
}

+ (BOOL)isMouseCoalescingEnabled
{
    return YES;
}

+ (void)setMouseCoalescingEnabled:(BOOL)enabled
{
}

// a CGEvent with the same type, location, flags, key and characters, owned by the autorelease pool
- (CGEventRef)CGEvent
{
    NSEventType type = [self type];
    NSPoint where = [NSEvent mouseLocation];
    CGFloat top = NSMaxY([[[NSScreen screens] firstObject] frame]);
    CGEventRef event = NULL;

    if (type == NSKeyDown || type == NSKeyUp) {
        NSString *characters = [self characters];
        UniChar buffer[32];
        NSUInteger length = MIN([characters length], (NSUInteger)32);

        event = CGEventCreateKeyboardEvent(NULL, [self keyCode], type == NSKeyDown);
        [characters getCharacters:buffer range:NSMakeRange(0, length)];
        if (event != NULL && length > 0)
            CGEventKeyboardSetUnicodeString(event, length, buffer);
    } else if (type == NSFlagsChanged) {
        event = CGEventCreateKeyboardEvent(NULL, [self keyCode], true);
    } else {
        CGEventType mouseType = kCGEventMouseMoved;
        CGMouseButton button = kCGMouseButtonLeft;

        switch (type) {
        case NSLeftMouseDown: mouseType = kCGEventLeftMouseDown; break;
        case NSLeftMouseUp: mouseType = kCGEventLeftMouseUp; break;
        case NSLeftMouseDragged: mouseType = kCGEventLeftMouseDragged; break;
        case NSRightMouseDown: mouseType = kCGEventRightMouseDown; button = kCGMouseButtonRight; break;
        case NSRightMouseUp: mouseType = kCGEventRightMouseUp; button = kCGMouseButtonRight; break;
        case NSRightMouseDragged: mouseType = kCGEventRightMouseDragged; button = kCGMouseButtonRight; break;
        case NSOtherMouseDown: mouseType = kCGEventOtherMouseDown; button = kCGMouseButtonCenter; break;
        case NSOtherMouseUp: mouseType = kCGEventOtherMouseUp; button = kCGMouseButtonCenter; break;
        case NSOtherMouseDragged: mouseType = kCGEventOtherMouseDragged; button = kCGMouseButtonCenter; break;
        default: break;
        }
        // CGEvent locations are top-left based, NSEvent's screen locations bottom-left
        event = CGEventCreateMouseEvent(NULL, mouseType, CGPointMake(where.x, top - where.y), button);
    }
    if (event == NULL)
        return NULL;
    CGEventSetFlags(event, (CGEventFlags)[self modifierFlags]);
    return (CGEventRef)CFAutorelease(event);
}

+ (NSEvent *)eventWithCGEvent:(CGEventRef)cgEvent
{
    CGEventType type;
    NSUInteger flags;
    CGPoint where;
    CGFloat top = NSMaxY([[[NSScreen screens] firstObject] frame]);
    NSPoint location;
    NSWindow *window = [NSApp keyWindow];

    if (cgEvent == NULL)
        return nil;
    type = CGEventGetType(cgEvent);
    flags = (NSUInteger)CGEventGetFlags(cgEvent);
    where = CGEventGetLocation(cgEvent);
    location = NSMakePoint(where.x, top - where.y);
    if (window != nil)
        location = [window convertScreenToBase:location];
    if (type == kCGEventKeyDown || type == kCGEventKeyUp || type == kCGEventFlagsChanged) {
        unsigned short keyCode = (unsigned short)CGEventGetIntegerValueField(cgEvent, kCGKeyboardEventKeycode);
        NSEventType nsType = type == kCGEventKeyDown ? NSKeyDown : type == kCGEventKeyUp ? NSKeyUp : NSFlagsChanged;

        return [NSEvent keyEventWithType:nsType location:location modifierFlags:(unsigned)flags timestamp:[[NSProcessInfo processInfo] systemUptime]
            windowNumber:(int)[window windowNumber] context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:keyCode];
    }
    switch (type) {
    case kCGEventLeftMouseDown:
        return [NSEvent mouseEventWithType:NSLeftMouseDown location:location modifierFlags:flags timestamp:[[NSProcessInfo processInfo] systemUptime]
            windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1];
    case kCGEventLeftMouseUp:
        return [NSEvent mouseEventWithType:NSLeftMouseUp location:location modifierFlags:flags timestamp:[[NSProcessInfo processInfo] systemUptime]
            windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:0];
    case kCGEventRightMouseDown:
        return [NSEvent mouseEventWithType:NSRightMouseDown location:location modifierFlags:flags timestamp:[[NSProcessInfo processInfo] systemUptime]
            windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1];
    case kCGEventRightMouseUp:
        return [NSEvent mouseEventWithType:NSRightMouseUp location:location modifierFlags:flags timestamp:[[NSProcessInfo processInfo] systemUptime]
            windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:0];
    default:
        return [NSEvent mouseEventWithType:NSMouseMoved location:location modifierFlags:flags timestamp:[[NSProcessInfo processInfo] systemUptime]
            windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0];
    }
}

@end
