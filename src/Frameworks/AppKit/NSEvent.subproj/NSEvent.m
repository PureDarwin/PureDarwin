/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2010 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSEvent.h>
#import <AppKit/NSEvent_mouse.h>
#import <AppKit/NSEvent_keyboard.h>
#import <AppKit/NSEvent_periodic.h>
#import <AppKit/NSEvent_other.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSDisplay.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSException.h>

// local event monitors, run on every event the application dequeues
@interface NSEventLocalMonitor : NSObject {
@public
   NSEventMask _mask;
   NSEvent *(^_handler)(NSEvent *);
}
@end

@implementation NSEventLocalMonitor
-(void)dealloc {
   [_handler release];
   [super dealloc];
}
@end

static NSMutableArray *localMonitors;
static NSUInteger pressedButtons;

@implementation NSEvent

// events never change once made, so a copy is the same event
-copyWithZone:(NSZone *)zone {
   return [self retain];
}

+(id)addLocalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(NSEvent *(^)(NSEvent *event))block {
   NSEventLocalMonitor *monitor=[[NSEventLocalMonitor new] autorelease];

   monitor->_mask=mask;
   monitor->_handler=[block copy];
   if(localMonitors==nil)
    localMonitors=[NSMutableArray new];
   [localMonitors addObject:monitor];
   return monitor;
}

// no global monitors without a WindowServer event tap
+(id)addGlobalMonitorForEventsMatchingMask:(NSEventMask)mask handler:(void (^)(NSEvent *event))block {
   return nil;
}

+(void)removeMonitor:(id)monitor {
   [localMonitors removeObjectIdenticalTo:monitor];
}

// what a dequeued event becomes after the monitors (nil when one swallowed it), tracks mouse buttons on the way
+(NSEvent *)_runLocalMonitors:(NSEvent *)event {
   switch([event type]){
    case NSLeftMouseDown: pressedButtons|=1; break;
    case NSLeftMouseUp: pressedButtons&=~1u; break;
    case NSRightMouseDown: pressedButtons|=2; break;
    case NSRightMouseUp: pressedButtons&=~2u; break;
    case NSOtherMouseDown: pressedButtons|=4; break;
    case NSOtherMouseUp: pressedButtons&=~4u; break;
    default: break;
   }
   NSArray *monitors=[[localMonitors copy] autorelease];

   for(NSEventLocalMonitor *monitor in monitors){
    if(event==nil)
     break;
    if(monitor->_mask & NSEventMaskFromType([event type]))
     event=monitor->_handler(event);
   }
   return event;
}

+(NSUInteger)pressedMouseButtons {
   return pressedButtons;
}


+(NSTimeInterval)doubleClickInterval {
   return [[NSDisplay currentDisplay] doubleClickInterval];
}

// macOS defaults, there is no keyboard preference pane to change them
+(NSTimeInterval)keyRepeatDelay {
   return 0.5;
}

+(NSTimeInterval)keyRepeatInterval {
   return 0.083;
}

+(NSPoint)mouseLocation {
   return [[NSDisplay currentDisplay] mouseLocation];
}

+(NSEventModifierFlags)modifierFlags {
   return [[NSDisplay currentDisplay] currentModifierFlags];
}

-initWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(unsigned)modifierFlags window:(NSWindow *)window {
   _type=type;
   _timestamp=[NSDate timeIntervalSinceReferenceDate];
   _locationInWindow=location;
   _modifierFlags=modifierFlags;
   _windowNumber=[window windowNumber];
   return self;
}

-(void)dealloc {
   [super dealloc];
}

+(NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(unsigned)modifierFlags window:(NSWindow *)window clickCount:(int)clickCount deltaX:(float)deltaX deltaY:(float)deltaY{
   return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:modifierFlags window:window clickCount:clickCount deltaX:deltaX deltaY:deltaY] autorelease];
}

+(NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(unsigned)modifierFlags window:(NSWindow *)window deltaY:(float)deltaY {
   return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:modifierFlags window:window deltaY:deltaY] autorelease];
}

+(NSEvent *)enterExitEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSUInteger)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext *)context eventNumber:(NSInteger)eventNumber trackingNumber:(NSInteger)tracking userData:(void *)userData {
 return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:flags timestamp:timestamp windowNumber:windowNumber context:context eventNumber:eventNumber trackingNumber:tracking userData:userData] autorelease];
}

+(NSEvent *)mouseEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSUInteger)modifierFlags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNumber context:(NSGraphicsContext*)context eventNumber:(NSInteger)eventNumber clickCount:(NSInteger)clickCount pressure:(float)pressure {
   return [[[NSEvent_mouse alloc] initWithType:type location:location modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:context eventNumber:eventNumber clickCount:clickCount pressure:pressure] autorelease];
}

+(NSEvent *)keyEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(unsigned)modifierFlags window:(NSWindow *)window characters:(NSString *)characters charactersIgnoringModifiers:(NSString *)charactersIgnoringModifiers isARepeat:(BOOL)isARepeat keyCode:(unsigned short)keyCode {
   return [[[NSEvent_keyboard alloc] initWithType:type location:location modifierFlags:modifierFlags window:window characters:characters charactersIgnoringModifiers:charactersIgnoringModifiers isARepeat:isARepeat keyCode:keyCode] autorelease];
}

+(NSEvent *)keyEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(unsigned int)modifierFlags timestamp:(NSTimeInterval)timestamp windowNumber:(int)windowNumber context:(NSGraphicsContext *)context characters:(NSString *)characters charactersIgnoringModifiers:(NSString *)charactersIgnoringModifiers isARepeat:(BOOL)isARepeat keyCode:(unsigned short)keyCode {   
   return [[[NSEvent_keyboard alloc] initWithType:type location:location modifierFlags:modifierFlags window:[NSApp windowWithWindowNumber:windowNumber] characters:characters charactersIgnoringModifiers:charactersIgnoringModifiers isARepeat:isARepeat keyCode:keyCode] autorelease];
}

+(NSEvent *)otherEventWithType:(NSEventType)type location:(NSPoint)location modifierFlags:(NSUInteger)flags timestamp:(NSTimeInterval)timestamp windowNumber:(NSInteger)windowNum context:(NSGraphicsContext *)context subtype:(short)subtype data1:(NSInteger)data1 data2:(NSInteger)data2 {
   return [[NSEvent_other alloc] initWithType:type location:location modifierFlags:flags timestamp:timestamp windowNumber:windowNum context:context subtype:subtype data1:data1 data2:data2];
}


-(NSEventType)type {
   return _type;
}

-(NSTimeInterval)timestamp {
   return _timestamp;
}

-(NSPoint)locationInWindow {
   return _locationInWindow;
}

-(NSEventModifierFlags)modifierFlags {
   return _modifierFlags;
}

-(NSWindow *)window {
   return [NSApp windowWithWindowNumber:[self windowNumber]];
}

-(NSInteger)windowNumber {
    return _windowNumber;
}

-(NSInteger)clickCount {
   return 0;
}

-(CGFloat)deltaX {
   return 0;
}

-(CGFloat)deltaY {
   return 0;
}

-(CGFloat)deltaZ {
   return 0;
}

-(NSInteger)buttonNumber {
   switch(_type){
    case NSRightMouseDown: case NSRightMouseUp: case NSRightMouseDragged:
     return 1;
    case NSOtherMouseDown: case NSOtherMouseUp: case NSOtherMouseDragged:
     return 2;
    default:
     return 0;
   }
}

// wheel deltas are in lines
-(BOOL)hasPreciseScrollingDeltas {
   return NO;
}

-(CGFloat)scrollingDeltaX {
   return [self deltaX];
}

-(CGFloat)scrollingDeltaY {
   return [self deltaY];
}

-(BOOL)isARepeat {
   return NO;
}

-(NSString *)characters {
   return nil;
}

-(NSString *)charactersIgnoringModifiers {
   return nil;
}

-(unsigned short)keyCode {
   return 0xFFFF;
}

-(NSString *)description {
    return [NSString stringWithFormat:@"<NSEvent: type=%d loc=(%f,%f) time=%f flags=0x%X win=%p winNum=%d",[self type], [self locationInWindow].x, [self locationInWindow].y, [self timestamp], [self modifierFlags], [self window], [self windowNumber]];
}


static NSTimer *_periodicTimer=nil;

+(void)_periodicDelay:(NSTimer *)timer {
   NSTimeInterval period=[[timer userInfo] doubleValue];

   [_periodicTimer invalidate];
   [_periodicTimer release];

   _periodicTimer=[[NSTimer timerWithTimeInterval:period
     target:self selector:@selector(_periodicEvent:) userInfo:nil
     repeats:YES] retain];

   [[NSRunLoop currentRunLoop] addTimer:_periodicTimer forMode:NSEventTrackingRunLoopMode];
}

+(void)_periodicEvent:(NSTimer *)timer {
   NSEvent *event=[[[NSEvent_periodic alloc] initWithType:NSPeriodic location:NSMakePoint(0,0) modifierFlags:0 window:nil] autorelease];

   [[NSDisplay currentDisplay] postEvent:event atStart:NO];
   [[NSDisplay currentDisplay] discardEventsMatchingMask:NSPeriodicMask beforeEvent:event];
}


+(void)startPeriodicEventsAfterDelay:(NSTimeInterval)delay withPeriod:(NSTimeInterval)period {
   NSNumber *userInfo=[NSNumber numberWithDouble:period];

   if(_periodicTimer!=nil)
     [NSException raise:NSInternalInconsistencyException format:@"periodic events already enabled"];

   _periodicTimer=[[NSTimer timerWithTimeInterval:delay
     target:self selector:@selector(_periodicDelay:) userInfo:userInfo
     repeats:NO] retain];

   [[NSRunLoop currentRunLoop] addTimer:_periodicTimer forMode:NSEventTrackingRunLoopMode];
}

+(void)stopPeriodicEvents {
   [_periodicTimer invalidate];
   [_periodicTimer release];
   _periodicTimer=nil;
}

-(short)subtype {
   [NSException raise:NSInternalInconsistencyException format:@"No event subtype in %@",[self class]];
   return 0;
}

-(NSInteger)data1 {
   [NSException raise:NSInternalInconsistencyException format:@"No event data1 in %@",[self class]];
   return 0;
}

-(NSInteger)data2 {
   [NSException raise:NSInternalInconsistencyException format:@"No event data2 in %@",[self class]];
   return 0;
}

-(void *)userData {
	[NSException raise:NSInternalInconsistencyException format:@"No userData in %@",[self class]];
	return 0;
}

-(NSInteger)trackingNumber {
	[NSException raise:NSInternalInconsistencyException format:@"No trackingNumber in %@",[self class]];
	return 0;
}

-(NSTrackingArea *)trackingArea {
	[NSException raise:NSInternalInconsistencyException format:@"No trackingArea in %@",[self class]];
    return nil;
}

@end

// binaries built against the earlier non-inline header still import this name
NSEventMask PDExportedNSEventMaskFromType(NSEventType type) __asm__("_NSEventMaskFromType");
NSEventMask PDExportedNSEventMaskFromType(NSEventType type){
   return NSEventMaskFromType(type);
}

