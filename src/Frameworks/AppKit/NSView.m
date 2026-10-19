/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2009-2010 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSView.h>
#include <stdio.h>
#include <stdlib.h>
#import <AppKit/NSTextInputContext.h>
#import <objc/runtime.h>
#import <AppKit/NSLayoutConstraint.h>
#import <objc/runtime.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSCursor.h>
#import <AppKit/NSCursorRect.h>
#import <AppKit/NSTrackingArea.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSClipView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSGraphicsContextFunctions.h>
#import <AppKit/NSDraggingManager.h>
#import <AppKit/NSDragging.h>
#import <AppKit/NSPrintOperation.h>
#import <AppKit/NSPrintInfo.h>
#import <Foundation/NSKeyedArchiver.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSClipView.h>
#import <AppKit/NSScreen.h>
#import <Foundation/NSProcessInfo.h>
#import <AppKit/NSObject+BindingSupport.h>
#import <Onyx2D/O2Context.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSViewBackingLayer.h>
#import <CoreGraphics/CGWindow.h>
#import <QuartzCore/CALayerContext.h>
#import <QuartzCore/CATransaction.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer-Private.h>

NSString * const NSViewFrameDidChangeNotification=@"NSViewFrameDidChangeNotification";
// posted with the frame change, the window position isn't tracked separately for OpenGL surfaces
NSString * const NSViewGlobalFrameDidChangeNotification=@"NSViewGlobalFrameDidChangeNotification";
NSString * const NSViewBoundsDidChangeNotification=@"NSViewBoundsDidChangeNotification";
NSString * const NSViewFocusDidChangeNotification=@"NSViewFocusDidChangeNotification";

@interface NSView(NSView_forward)
-(CGAffineTransform)transformFromWindow;
-(CGAffineTransform)transformToWindow;
-(CGAffineTransform)transformToLayer;
-(void)_trackingAreasChanged;
@end

void _NSAppearanceDrawView(NSView *view, void (^draw)(void));

@interface CALayer (PDContentsWithoutRender)
-(void)_setContentsWithoutRender:(id)value;
@end

@implementation NSView

static BOOL NSViewLayersEnabled=NO;

+(void)initialize {
   NSViewLayersEnabled=[[NSUserDefaults standardUserDefaults] boolForKey:@"NSViewLayersEnabled"];
}

+(NSView *)focusView {
   return [NSCurrentFocusStack() lastObject];
}

+(NSMenu *)defaultMenu {
   return nil;
}

+(NSFocusRingType)defaultFocusRingType {
   return NSFocusRingTypeExterior;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
}

-initWithCoder:(NSCoder *)coder {
   [super initWithCoder:coder];

   if([coder allowsKeyedCoding]){
    NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;
    unsigned           vFlags=[keyed decodeIntForKey:@"NSvFlags"];

    _frame=NSZeroRect;
    if([keyed containsValueForKey:@"NSFrame"])
     _frame=[keyed decodeRectForKey:@"NSFrame"];
    else if([keyed containsValueForKey:@"NSFrameSize"])
     _frame.size=[keyed decodeSizeForKey:@"NSFrameSize"];

    _bounds.origin=NSMakePoint(0,0);
    _bounds.size=_frame.size;
    _window=nil;
    _superview=nil;
    _subviews=[NSMutableArray new];
    _postsNotificationOnFrameChange=YES;
    _postsNotificationOnBoundsChange=YES;
    _autoresizingMask=vFlags&0x3F;
    if([keyed containsValueForKey:@"NSvFlags"])
     _autoresizesSubviews=(vFlags&0x100)?YES:NO;
// Despite the fact it appears _autoresizesSubviews is encoded in the flags, it should always be ON
// *** Do not turn off or base on the flags unless you have an alternative for enabling it when it
// should be.
     _autoresizesSubviews=YES;
    _isHidden=(vFlags&0x80000000)?YES:NO;
    _tag= 0; // IB assigns a default tag id of 0 - which is different from the default in the docs.
    if([keyed containsValueForKey:@"NSTag"])
     _tag=[keyed decodeIntForKey:@"NSTag"];

    // Subviews come in from the nib in back to front order
    [_subviews addObjectsFromArray:[keyed decodeObjectForKey:@"NSSubviews"]];
    [_subviews makeObjectsPerformSelector:@selector(viewWillMoveToSuperview:) withObject:self];
    [_subviews makeObjectsPerformSelector:@selector(_setSuperview:) withObject:self];
    [_subviews makeObjectsPerformSelector:@selector(viewDidMoveToSuperview)];

    _needsDisplay=YES;
    _invalidRectCount=0;
    _invalidRects=NULL;
    _trackingAreas=[[NSMutableArray alloc] init];
    [self setWantsLayer:[keyed decodeBoolForKey:@"NSViewIsLayerTreeHost"]];

    _layerContentsRedrawPolicy=[keyed decodeIntegerForKey:@"NSViewLayerContentsRedrawPolicy"];

    _contentFilters=[[keyed decodeObjectForKey:@"NSViewContentFilters"] retain];
    // auto Layout: the constraints IB archived on this view, and whether its frame was meant to come from them
    [self addConstraints:[keyed decodeObjectForKey:@"NSViewConstraints"]];
    if([keyed containsValueForKey:@"NSDoNotTranslateAutoresizingMask"])
     [self setTranslatesAutoresizingMaskIntoConstraints:![keyed decodeBoolForKey:@"NSDoNotTranslateAutoresizingMask"]];
   }
   else {
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class], [coder class]];
   }

   return self;
}

-init {
   return [self initWithFrame:NSMakeRect(0,0,1,1)];
}

-initWithFrame:(NSRect)frame {
   _frame=frame;
   _bounds.origin=NSMakePoint(0,0);
   _bounds.size=frame.size;
   _window=nil;
   _menu=nil;
   _superview=nil;
   _subviews=[NSMutableArray new];
   _postsNotificationOnFrameChange=YES;
   _postsNotificationOnBoundsChange=YES;
   _autoresizesSubviews=YES;
   _autoresizingMask=NSViewNotSizable;
   _tag=-1; // according to the docs - loading from a nib gets a default of 0.
   _needsDisplay=YES;
   _invalidRectCount=0;
   _invalidRects=NULL;
   _trackingAreas=[[NSMutableArray alloc] init];

   _validTransforms=NO;
   _transformFromWindow=CGAffineTransformIdentity;
   _transformToWindow=CGAffineTransformIdentity;
   _transformToLayer=CGAffineTransformIdentity;

   return self;
}

-(void)dealloc {

    // Do this first?
    [self _unbindAllBindings];

   _window=nil;
   [_menu release];

   _superview=nil;
   [_subviews makeObjectsPerformSelector:@selector(_setSuperview:) withObject:nil];

   [_subviews release];
   [_draggedTypes release];
   [_trackingAreas release];
   [_contentFilters release];

   if(_invalidRects!=NULL)
    NSZoneFree(NULL,_invalidRects);

    if (_rectsBeingRedrawn!=NULL) {
        NSZoneFree(NULL, _rectsBeingRedrawn);
    }
   [_layer release];

   [_layerContext invalidate];
   [_layerContext release];

   [super dealloc];
}

static void invalidateTransform(NSView *self){
   self->_validTransforms=NO;
   self->_validTrackingAreas=NO;

   for(NSView *check in self->_subviews)
    invalidateTransform(check);

}

static CGAffineTransform concatViewTransform(CGAffineTransform result,NSView *view,NSView *superview,BOOL doFrame,BOOL flip){
   NSRect bounds=[view bounds];
   NSRect frame=[view frame];

   if(doFrame)
   result=CGAffineTransformTranslate(result,frame.origin.x,frame.origin.y);

    // Apply bounds scaling to fit in the frame
    CGAffineTransform scale = CGAffineTransformMakeScale(NSWidth(frame)/NSWidth(bounds), NSHeight(frame)/NSHeight(bounds));
    result=CGAffineTransformConcat(scale,result);

   if(flip){
    CGAffineTransform flip=CGAffineTransformMake(1,0,0,-1,0,bounds.size.height);

    result=CGAffineTransformConcat(flip,result);
   }
   result=CGAffineTransformTranslate(result,-bounds.origin.x,-bounds.origin.y);

   return result;
}

-(CGAffineTransform)createTransformToWindow {
   CGAffineTransform result;
   NSView *superview=[self superview];
   BOOL    doFrame=YES;
   BOOL    flip;

   if(superview==nil){
    result=CGAffineTransformIdentity;
    flip=[self isFlipped];
   }
   else {
    result=[superview transformToWindow];
    flip=([self isFlipped]!=[superview isFlipped]);
   }

   result=concatViewTransform(result,self,superview,doFrame,flip);

   return result;
}

-(CGAffineTransform)createTransformToLayer {
   CGAffineTransform result=CGAffineTransformIdentity;
   NSRect bounds=[self bounds];

   if([self isFlipped]){
    CGAffineTransform flip=CGAffineTransformMake(1,0,0,-1,0,bounds.size.height);

    result=CGAffineTransformConcat(flip,result);
   }
   result=CGAffineTransformTranslate(result,-bounds.origin.x,-bounds.origin.y);

   return result;
}

-(NSRect)calculateVisibleRect {
   if([self isHiddenOrHasHiddenAncestor])
    return NSZeroRect;

   if([self superview]==nil)
    return [self bounds];
   else {
    NSRect result=[[self superview] visibleRect];

    result=[self convertRect:result fromView:[self superview]];

    result=NSIntersectionRect(result,[self bounds]);

    return result;
   }
}

static inline void configureLayerGeometry(NSView *self){
   CALayer *layer=self->_layer;

   if(layer==nil)
    return;

   [CATransaction begin];
   [CATransaction setDisableActions:YES];

   [layer setAnchorPoint:CGPointMake(0,0)];
   // a backing layer is flipped where the view's flippedness differs from its superview's,
   // since layer geometry nests the way view coordinates do
   if([layer isKindOfClass:[NSViewBackingLayer class]])
    [layer setGeometryFlipped:[self isFlipped]!=[self->_superview isFlipped]];

   if([layer superlayer]==nil)
    [layer setPosition:CGPointMake(0,0)];
   else
    [layer setPosition:self->_frame.origin];

   [layer setBounds:self->_bounds];
   [CATransaction commit];
}

static inline void buildTransformsIfNeeded(NSView *self) {
   if(!self->_validTransforms){
    self->_transformToWindow=[self createTransformToWindow];
    self->_transformFromWindow=CGAffineTransformInvert(self->_transformToWindow);
    self->_transformToLayer=[self createTransformToLayer];
    self->_validTransforms=YES;

    self->_visibleRect=[self calculateVisibleRect];
    configureLayerGeometry(self);
   }
}

-(CGAffineTransform)transformFromWindow {
   buildTransformsIfNeeded(self);

   return _transformFromWindow;
}

-(CGAffineTransform)transformToWindow {
   buildTransformsIfNeeded(self);

   return _transformToWindow;
}


-(CGAffineTransform)transformToLayer {
   buildTransformsIfNeeded(self);

   return _transformToLayer;
}

-(NSRect)frame {
   return _frame;
}

-(CGFloat)frameRotation {
   return _frameRotation;
}

-(CGFloat)frameCenterRotation {
   return [self frameRotation];
}

-(NSRect)bounds {
   return _bounds;
}

-(CGFloat)boundsRotation {
   return _boundsRotation;
}

-(BOOL)isRotatedFromBase {
   for(NSView *view=self;view!=nil;view=[view superview])
    if([view frameRotation]!=0 || [view boundsRotation]!=0)
     return YES;
   return NO;
}

-(BOOL)isRotatedOrScaledFromBase {
   for(NSView *view=self;view!=nil;view=[view superview]){
    NSRect frame=[view frame],bounds=[view bounds];

    if([view frameRotation]!=0 || [view boundsRotation]!=0)
     return YES;
    if(!NSEqualSizes(frame.size,bounds.size))
     return YES;
   }
   return NO;
}

-(void)translateOriginToPoint:(NSPoint)point {
   NSRect bounds=[self bounds];

   [self setBoundsOrigin:NSMakePoint(NSMinX(bounds)-point.x,NSMinY(bounds)-point.y)];
}

-(void)rotateByAngle:(CGFloat)angle {
   [self setBoundsRotation:[self boundsRotation]+angle];
}

-(BOOL)postsFrameChangedNotifications {
   return _postsNotificationOnFrameChange;
}

-(BOOL)postsBoundsChangedNotifications {
   return _postsNotificationOnBoundsChange;
}

-(void)scaleUnitSquareToSize:(NSSize)size {
   NSRect bounds=[self bounds];

   if(size.width==0 || size.height==0)
    return;
   bounds.origin.x/=size.width;
   bounds.origin.y/=size.height;
   bounds.size.width/=size.width;
   bounds.size.height/=size.height;
   [self setBounds:bounds];
}

-(NSWindow *)window {
   return _window;
}

-(NSView *)superview {
   return _superview;
}

-(BOOL)isDescendantOf:(NSView *)other {
   NSView *check=self;

   do {
    if(check==other)
     return YES;

    check=[check superview];
   }while(check!=nil);

   return NO;
}

-(NSView *)ancestorSharedWithView:(NSView *)view {
   for(NSView *mine=self;mine!=nil;mine=[mine superview])
    if(view==mine || [view isDescendantOf:mine])
     return mine;
   return nil;
}

-(NSScrollView *)enclosingScrollView {
   id result=[self superview];

   for(;result!=nil;result=[result superview])
    if([result isKindOfClass:[NSScrollView class]])
     return result;

   return nil;
}

-(NSRect)adjustScroll:(NSRect)toRect {
   return toRect;
}

-(NSArray *)subviews {
   return _subviews;
}

-(BOOL)autoresizesSubviews {
   return _autoresizesSubviews;
}

-(unsigned)autoresizingMask {
   return _autoresizingMask;
}

-(NSFocusRingType)focusRingType {
   return _focusRingType;
}

-(int)tag {
   return _tag;
}

-(BOOL)isFlipped {
   return NO;
}

-(BOOL)isOpaque {
   return NO;
}

-(CGFloat)alphaValue {
   NSNumber *value=objc_getAssociatedObject(self,@selector(alphaValue));

   return value?[value doubleValue]:1.0;
}

-(void)setAlphaValue:(CGFloat)alpha {
   alpha=MAX(0,MIN(1,alpha));
   objc_setAssociatedObject(self,@selector(alphaValue),[NSNumber numberWithDouble:alpha],OBJC_ASSOCIATION_RETAIN);
   [_layer setOpacity:alpha];
   [self setNeedsDisplay:YES];
}

-(int)gState {
   return 0;
}

-(NSRect)visibleRect {
   buildTransformsIfNeeded(self);

   return _visibleRect;
}

-(BOOL)wantsDefaultClipping {
   return YES;
}

-(NSBitmapImageRep *)bitmapImageRepForCachingDisplayInRect:(NSRect)rect {
   NSRect pixels=[self convertRectToBacking:rect];
   NSBitmapImageRep *rep=[[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
      pixelsWide:MAX(1,(NSInteger)ceil(NSWidth(pixels))) pixelsHigh:MAX(1,(NSInteger)ceil(NSHeight(pixels)))
      bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
      bytesPerRow:0 bitsPerPixel:32] autorelease];

   [rep setSize:rect.size];
   return rep;
}

-(void)cacheDisplayInRect:(NSRect)rect toBitmapImageRep:(NSBitmapImageRep *)imageRep {
   NSGraphicsContext *context=[NSGraphicsContext graphicsContextWithBitmapImageRep:imageRep];

   if(context==nil)
    return;
   [self displayRectIgnoringOpacity:rect inContext:context];
}

-(BOOL)isHidden {
   return _isHidden;
}

-(BOOL)isHiddenOrHasHiddenAncestor {
   return _isHidden || [_superview isHiddenOrHasHiddenAncestor];
}

-(void)setHidden:(BOOL)flag {
    flag=flag?YES:NO;

    if (_isHidden != flag) {
        invalidateTransform(self);
        if ((_isHidden = flag)) {
            id view=[_window firstResponder];

            if ([view isKindOfClass:[NSView class]])
                for (; view; view = [view superview]) {
                    if (self==view) {
                        [_window makeFirstResponder:[self nextValidKeyView]];
                        break;
                    }
                }
        }

        [[self superview] setNeedsDisplay:YES];

        if(_isHidden)
            [self viewDidHide];
        else
            [self viewDidUnhide];
   }
}

-(void)viewDidHide {
    // do nothing?
}

-(void)viewDidUnhide {
    // do nothing?
}

-(BOOL)canBecomeKeyView {
   return [self acceptsFirstResponder] && ![self isHiddenOrHasHiddenAncestor];
}

// Cocoa does that for views, despite the "The default implementation returns NO" from the documentation
-(BOOL)needsPanelToBecomeKey {
    return [self acceptsFirstResponder];
}

-(NSView *)nextKeyView {
   return _nextKeyView;
}

-(NSView *)nextValidKeyView {
   NSView *result=[self nextKeyView];

    while(result!=nil && ![result canBecomeKeyView]) {
        // prevent an infinite loop
        if(result==self)
            return nil;

        result=[result nextKeyView];
    }


   return result;
}

-(NSView *)previousKeyView {
   return _previousKeyView;
}

-(NSView *)previousValidKeyView {
    NSView *result=[self previousKeyView];

    while(result!=nil && ![result canBecomeKeyView]) {
        // prevent an infinite loop
        if(result==self)
            return nil;
        result=[result previousKeyView];
    }

    return result;
}

-(NSMenu *)menu {
   return _menu;
}

-(NSMenu *)menuForEvent:(NSEvent *)event {
   NSMenu *result=[self menu];

   if(result==nil) {
    result=[[self class] defaultMenu];

    if(result) {
     NSArray *itemArray=[result itemArray];
     int i,count=[itemArray count];
     for(i=0;i<count;i++) {
      NSMenuItem *item = [itemArray objectAtIndex:i];
      [item setTarget:self];
     }
    }
   }

   return result;
}

-(NSMenuItem *)enclosingMenuItem {
   // menu items here draw themselves, a custom view finds its item through its menu
   for(NSView *view=self;view!=nil;view=[view superview]){
    NSMenuItem *item=objc_getAssociatedObject(view,@selector(enclosingMenuItem));

    if(item!=nil)
     return item;
   }
   return nil;
}

-(NSString *)toolTip {
   NSUInteger i,count=[_trackingAreas count];
   NSString *toolTip=nil;

   // In case there's more than one ToolTip the behavior
   // is undocumented. Return the first one.
   for(i=0;i<count;i++){
    NSTrackingArea *area=[_trackingAreas objectAtIndex:i];

    if([area _isToolTip]==YES){
     toolTip=[area owner];
     break;
    }
   }

   return toolTip;
}

-viewWithTag:(int)tag {
   int i,count=[_subviews count];

   if(_tag==tag)
    return self;

   for(i=0;i<count;i++){
    NSView *view=[[_subviews objectAtIndex:i] viewWithTag:tag];

    if(view!=nil)
     return view;
   }

   return nil;
}

-(NSView *)hitTest:(NSPoint)point
{
    if(_isHidden) {
        return nil;
    }

    point = [self convertPoint:point fromView:[self superview]];

    if(NSMouseInRect(point, [self visibleRect], [self isFlipped]) == NO){
        return nil;
    } else {
        // Subviews are ordered back to front so we need to go
        // front to back in order to hit test correctly.
        NSArray *subviews = [self subviews];
        int      count = [subviews count];

        while (--count >= 0) {
            NSView *check = [subviews objectAtIndex: count];
            NSView *hit = [check hitTest: point];

            if (hit != nil) {
                return hit;
            }
        }
    }
    return self;
}

-(NSPoint)convertPoint:(NSPoint)point fromView:(NSView *)viewOrNil {
   NSView           *fromView=(viewOrNil!=nil)?viewOrNil:[[self window] _backgroundView];
   CGAffineTransform toWindow=[fromView transformToWindow];
   CGAffineTransform fromWindow=[self transformFromWindow];

   return CGPointApplyAffineTransform(CGPointApplyAffineTransform(point,toWindow),fromWindow);
}

-(NSPoint)convertPoint:(NSPoint)point toView:(NSView *)viewOrNil {
   NSView           *toView=(viewOrNil!=nil)?viewOrNil:[[self window] _backgroundView];
   CGAffineTransform toWindow=[self transformToWindow];
   CGAffineTransform fromWindow=[toView transformFromWindow];

   return CGPointApplyAffineTransform(CGPointApplyAffineTransform(point,toWindow),fromWindow);
}

-(NSSize)convertSize:(NSSize)size fromView:(NSView *)viewOrNil {
   NSView           *fromView=(viewOrNil!=nil)?viewOrNil:[[self window] _backgroundView];
   CGAffineTransform toWindow=[fromView transformToWindow];
   CGAffineTransform fromWindow=[self transformFromWindow];

   return CGSizeApplyAffineTransform(CGSizeApplyAffineTransform(size,toWindow),fromWindow);
}

-(NSSize)convertSize:(NSSize)size toView:(NSView *)viewOrNil {
   NSView           *toView=(viewOrNil!=nil)?viewOrNil:[[self window] _backgroundView];
   CGAffineTransform toWindow=[self transformToWindow];
   CGAffineTransform fromWindow=[toView transformFromWindow];

   return CGSizeApplyAffineTransform(CGSizeApplyAffineTransform(size,toWindow),fromWindow);
}

-(NSRect)convertRect:(NSRect)rect fromView:(NSView *)viewOrNil {
   NSView           *fromView=(viewOrNil!=nil)?viewOrNil:[[self window] _backgroundView];
   CGAffineTransform toWindow=[fromView transformToWindow];
   CGAffineTransform fromWindow=[self transformFromWindow];
   NSPoint           point1=rect.origin;
   NSPoint           point2=NSMakePoint(NSMaxX(rect),NSMaxY(rect));

   point1= CGPointApplyAffineTransform(CGPointApplyAffineTransform(point1,toWindow),fromWindow);
   point2= CGPointApplyAffineTransform(CGPointApplyAffineTransform(point2,toWindow),fromWindow);
   if(point2.y<point1.y){
    float temp=point2.y;
    point2.y=point1.y;
    point1.y=temp;
   }

   return NSMakeRect(point1.x,point1.y,point2.x-point1.x,point2.y-point1.y);
}

-(NSRect)convertRect:(NSRect)rect toView:(NSView *)viewOrNil {
   NSView           *toView=(viewOrNil!=nil)?viewOrNil:[[self window] _backgroundView];
   CGAffineTransform toWindow=[self transformToWindow];
   CGAffineTransform fromWindow=[toView transformFromWindow];
   NSPoint           point1=rect.origin;
   NSPoint           point2=NSMakePoint(NSMaxX(rect),NSMaxY(rect));

   point1= CGPointApplyAffineTransform(CGPointApplyAffineTransform(point1,toWindow),fromWindow);
   point2= CGPointApplyAffineTransform(CGPointApplyAffineTransform(point2,toWindow),fromWindow);
   if(point2.y<point1.y){
    float temp=point2.y;
    point2.y=point1.y;
    point1.y=temp;
   }

   return NSMakeRect(point1.x,point1.y,point2.x-point1.x,point2.y-point1.y);
}

-(NSRect)centerScanRect:(NSRect)rect {
   float minx=floor(NSMinX(rect)+0.5);
   float miny=floor(NSMinY(rect)+0.5);
   float maxx=floor(NSMaxX(rect)+0.5);
   float maxy=floor(NSMaxY(rect)+0.5);

   return NSMakeRect(minx,miny,maxx-minx,maxy-miny);
}

// as on macOS, a frame change goes through -setFrameOrigin: and -setFrameSize:, so a
// subclass that overrides either one sees every resize
-(void)setFrame:(NSRect)frame {
   if(getenv("PD_CA_TRACE"))
    fprintf(stderr,"PD-CA view %s %p setFrame: %g %g %g %g\n",object_getClassName(self),self,frame.origin.x,frame.origin.y,frame.size.width,frame.size.height);
   if(!NSEqualPoints(frame.origin,_frame.origin))
    [self setFrameOrigin:frame.origin];
   if(!NSEqualSizes(frame.size,_frame.size))
    [self setFrameSize:frame.size];
}

// a hosted CAMetalLayer is a Wayland subsurface of the window at the view's rectangle, from the window's top left
static CGRect metalLayerRect(NSView *self){
   NSRect r=[self convertRect:[self bounds] toView:nil];

   return CGRectMake(r.origin.x,[[self window] frame].size.height-(r.origin.y+r.size.height),r.size.width,r.size.height);
}

static void updateMetalLayer(NSView *self){
   if(![self->_layer isKindOfClass:[CAMetalLayer class]])
    return;

   CAMetalLayer *layer=(CAMetalLayer *)self->_layer;

   if(self->_window==nil){
    [layer _pdDetach];
    return;
   }

   CGRect r=metalLayerRect(self);

   [layer setBounds:CGRectMake(0,0,r.size.width,r.size.height)];
   if([layer _pdIsAttached]){
    [layer setWindowOrigin:r.origin];
    [layer _pdSetViewSize:r.size];
   }
   else
    [layer _pdAttachToWindowNumber:[self->_window windowNumber] origin:r.origin size:r.size];
}

-(void)_setFrame:(NSRect)frame {
  // Cocoa does not post the notification if the frames are equal
   // Possible that resizeSubviewsWithOldSize is not called if the sizes are equal
   if(NSEqualRects(_frame,frame))
    return;

   NSRect oldFrame=_frame;
   NSSize oldSize=_bounds.size;

    if (_bounds.size.width == 0 || _bounds.size.height == 0) {
        // No valid current bounds value - just update it to use the frame size
        _bounds.size=frame.size;
    } else {
        // Get the bounds->frame transform
        CGAffineTransform transform=concatViewTransform(CGAffineTransformIdentity,self,nil,YES,NO);
        // ... and invert it so we can get the new bounds size from the new frame size
        transform = CGAffineTransformInvert(transform);

        _bounds.size=CGSizeApplyAffineTransform(frame.size, transform);
    }
    _frame=frame;

    [_window invalidateCursorRectsForView:self]; // this also invalidates tracking areas

   if(_autoresizesSubviews){
    [self resizeSubviewsWithOldSize:oldSize];
   }

    NSRect layerFrame=_frame;

    if(_superview!=nil)
        layerFrame=[_superview convertRect:layerFrame toView:nil];

    [_layerContext setFrame:layerFrame];

    updateMetalLayer(self);

   invalidateTransform(self);

   if(_superview!=nil && !NSContainsRect(_frame,oldFrame))
    [_superview setNeedsDisplayInRect:oldFrame];

   if(_postsNotificationOnFrameChange)
    [[NSNotificationCenter defaultCenter] postNotificationName:NSViewFrameDidChangeNotification object:self];
    [[NSNotificationCenter defaultCenter] postNotificationName:NSViewGlobalFrameDidChangeNotification object:self];
}

-(void)setFrameSize:(NSSize)size {
   if(getenv("PD_CA_TRACE"))
    fprintf(stderr,"PD-CA view %s %p setFrameSize: %g %g\n",object_getClassName(self),self,size.width,size.height);
   NSRect frame=_frame;

   frame.size=size;
   [self _setFrame:frame];
}

-(void)setFrameOrigin:(NSPoint)origin {
   NSRect frame=_frame;

   frame.origin=origin;
   [self _setFrame:frame];
}

-(void)setFrameRotation:(CGFloat)angle {
   _frameRotation=angle;
   invalidateTransform(self);
   [self setNeedsDisplay:YES];
}

-(void)setFrameCenterRotation:(CGFloat)angle {
   [self setFrameRotation:angle];
}

-(void)setBounds:(NSRect)bounds {
    if (!NSEqualRects(bounds, _bounds)) {
        _bounds=bounds;
        invalidateTransform(self);

        [_window invalidateCursorRectsForView:self]; // this also invalidates tracking areas

        if(_postsNotificationOnBoundsChange)
            [[NSNotificationCenter defaultCenter] postNotificationName:NSViewBoundsDidChangeNotification object:self];
    }
}

-(void)setBoundsSize:(NSSize)size {
   NSRect bounds=[self bounds];

   bounds.size=size;

   [self setBounds:bounds];
}

-(void)setBoundsOrigin:(NSPoint)origin {
   NSRect bounds=[self bounds];

   bounds.origin=origin;

   [self setBounds:bounds];
}

-(void)setBoundsRotation:(CGFloat)angle {
   _boundsRotation=angle;
   invalidateTransform(self);
   [self setNeedsDisplay:YES];
}

-(void)setPostsFrameChangedNotifications:(BOOL)flag {
   _postsNotificationOnFrameChange=flag;
}

-(void)setPostsBoundsChangedNotifications:(BOOL)flag {
   _postsNotificationOnBoundsChange=flag;
}

-(void)_setWindow:(NSWindow *)window {
    if (_window != window) {
        BOOL windowRecalulatesKeyViewLoop = [window autorecalculatesKeyViewLoop];
        if (windowRecalulatesKeyViewLoop)
            [self setNextKeyView:nil];

        [self viewWillMoveToWindow:window];

        _window=window;

        [_subviews makeObjectsPerformSelector:_cmd withObject:window];
        _validTrackingAreas=NO;
        [_window invalidateCursorRectsForView:self]; // this also invalidates tracking areas

        if (windowRecalulatesKeyViewLoop)
            [_window recalculateKeyViewLoop];

        [self viewDidMoveToWindow];
        updateMetalLayer(self);
    }
}

-(void)_setSuperview:superview {
   _superview=superview;

   [_window invalidateCursorRectsForView:self]; // this also invalidates tracking areas

   [self setNextResponder:superview];
}

-(void)_insertSubview:(NSView *)view atIndex:(NSUInteger)index {

   [view retain];
   if([view superview]==self)
    [_subviews removeObjectIdenticalTo:view];
   else{
    [view removeFromSuperview];

    [view _setWindow:_window];

    [view viewWillMoveToSuperview:self];
    [view _setSuperview:self];
   }

   if(index==NSNotFound)
    [_subviews addObject:view];
   else
    [_subviews insertObject:view atIndex:index];
   [view release];

   invalidateTransform(view);

   [self setNeedsDisplayInRect:[view frame]];

   [view viewDidMoveToSuperview];
   [self didAddSubview:view];

   if(_wantsLayer)
    [view setWantsLayer:YES];
}

-(void)addSubview:(NSView *)view {
   if(view==nil) // yes, this is silently ignored
    return;

   [self _insertSubview:view atIndex:NSNotFound];
}

-(void)addSubview:(NSView *)view positioned:(NSWindowOrderingMode)ordering relativeTo:(NSView *)relativeTo {
   NSUInteger index=[_subviews indexOfObjectIdenticalTo:relativeTo];

   if(index==NSNotFound)
    index=(ordering==NSWindowBelow)?0:NSNotFound;
   else
    index=(ordering==NSWindowBelow)?index:((index+1==[_subviews count])?NSNotFound:index+1);

   [self _insertSubview:view atIndex:index];
}

-(void)replaceSubview:(NSView *)oldView with:(NSView *)newView {
   NSUInteger index=[_subviews indexOfObjectIdenticalTo:oldView];

   [oldView retain];
   [oldView removeFromSuperview];
   [self _insertSubview:newView atIndex:index];
   [oldView release];
}

-(void)setSubviews:(NSArray *)array {
// This method marks as needing display per doc.s

   while([_subviews count])
    [[_subviews lastObject] removeFromSuperview];

   for(NSView *view in array){
    [self addSubview:view];
    [view setNeedsDisplay:YES];
}
}

-(void)sortSubviewsUsingFunction:(NSComparisonResult (*)(id, id, void *))compareFunction context:(void *)context {
   [_subviews sortUsingFunction:compareFunction context:context];
   [self setNeedsDisplay:YES];
}

-(void)didAddSubview:(NSView *)subview {
   // for subclasses
}

-(void)willRemoveSubview:(NSView *)subview {
   // for subclasses
}

-(void)setAutoresizesSubviews:(BOOL)flag {
   _autoresizesSubviews=flag;
}

-(void)setAutoresizingMask:(unsigned int)mask {
   _autoresizingMask=mask;
}

-(void)setFocusRingType:(NSFocusRingType)value {
   _focusRingType=value;
   [self setNeedsDisplay:YES];
}

-(void)setTag:(int)tag {
   _tag=tag;
}

-(void)_setPreviousKeyView:(NSView *)previous {
    _previousKeyView = previous;
}

-(void)setNextKeyView:(NSView *)next {
    if (next)
        [next _setPreviousKeyView:self];
    else
        [_nextKeyView _setPreviousKeyView:nil];

    _nextKeyView = next;
}

-(BOOL)acceptsFirstMouse:(NSEvent *)event {
   return NO;
}

-(BOOL)acceptsTouchEvents {
   return [objc_getAssociatedObject(self,@selector(acceptsTouchEvents)) boolValue];
}

-(void)setAcceptsTouchEvents:(BOOL)accepts {
   objc_setAssociatedObject(self,@selector(acceptsTouchEvents),[NSNumber numberWithBool:accepts],OBJC_ASSOCIATION_RETAIN);
}

-(BOOL)wantsRestingTouches {
   return [objc_getAssociatedObject(self,@selector(wantsRestingTouches)) boolValue];
}

-(void)setWantsRestingTouches:(BOOL)wants {
   objc_setAssociatedObject(self,@selector(wantsRestingTouches),[NSNumber numberWithBool:wants],OBJC_ASSOCIATION_RETAIN);
}

-(void)setToolTip:(NSString *)string {
   [self removeAllToolTips];
   if(string!=nil && ![string isEqualToString:@""])
    [self addToolTipRect:[self bounds] owner:string userData:NULL];
}

-(NSToolTipTag)addToolTipRect:(NSRect)rect owner:object userData:(void *)userData {
   NSTrackingArea *area=nil;

   area=[[NSTrackingArea alloc] _initWithRect:rect options:0 owner:object userData:userData retainUserData:NO isToolTip:YES isLegacy:NO];
   [_trackingAreas addObject:area];
   [area release];

   [self _trackingAreasChanged];

   return area;
}

-(void)removeToolTip:(NSToolTipTag)tag {
   [self removeTrackingArea:tag];
}

-(void)removeAllToolTips {
   NSInteger count=[_trackingAreas count];

   while(--count>=0){
    if([[_trackingAreas objectAtIndex:count] _isToolTip]==YES)
     [_trackingAreas removeObjectAtIndex:count];
   }

   [self _trackingAreasChanged];
}

-(void)addCursorRect:(NSRect)rect cursor:(NSCursor *)cursor {
   NSCursorRect *cursorRect=[[NSCursorRect alloc] initWithCursor:cursor];
   NSTrackingArea *area=nil;

   area=[[NSTrackingArea alloc] _initWithRect:rect options:NSTrackingCursorUpdate|NSTrackingActiveInKeyWindow owner:cursorRect userData:NULL retainUserData:NO isToolTip:NO isLegacy:YES];
   [_trackingAreas addObject:area];
   [area release];
   [cursorRect release];

   [self _trackingAreasChanged];
}

-(void)removeCursorRect:(NSRect)rect cursor:(NSCursor *)cursor {
   NSInteger count=[_trackingAreas count];

   while(--count>=0){
    NSTrackingArea *area=[_trackingAreas objectAtIndex:count];
    NSObject *candidate=[area owner];

    if([area _isLegacy]==YES &&
       [candidate isKindOfClass:[NSCursorRect class]]==YES &&
       [(NSCursorRect *)candidate cursor]==cursor){
     [_trackingAreas removeObjectAtIndex:count];
     break;
    }
   }

   [self _trackingAreasChanged];
}

-(void)discardCursorRects {
   NSInteger count=[_trackingAreas count];

   while(--count>=0){
    NSTrackingArea *area=[_trackingAreas objectAtIndex:count];

    if([area _isLegacy]==YES && ([area options]&NSTrackingCursorUpdate)){
     [_trackingAreas removeObjectAtIndex:count];
    }
   }

   [[self subviews] makeObjectsPerformSelector:_cmd];

   [self _trackingAreasChanged];
}

-(void)resetCursorRects {
   // do nothing
}

-(void)_collectTrackingAreasForWindowInto:(NSMutableArray *)collector {
   if(_isHidden==NO){
    NSUInteger  i,count;

    if(!_validTrackingAreas){
     /* We don't clear the tracking areas, they are managed by the view with add/remove
      */
     [self updateTrackingAreas];
     _validTrackingAreas=YES;
    }

    count=[_trackingAreas count];
    for(i=0;i<count;i++){
     NSTrackingArea *area=[_trackingAreas objectAtIndex:i];
     NSRect          rectOfInterest;

     rectOfInterest=NSIntersectionRect([area rect], [self bounds]);

     if(rectOfInterest.size.width>0. && rectOfInterest.size.height>0.){
      [area _setView:self];

      [area _setRectInWindow:[self convertRect:rectOfInterest toView:nil]];

      [collector addObject:[_trackingAreas objectAtIndex:i]];
     }
    }

    NSArray *subviews=[self subviews];
    // Collect subviews' areas _after_ collecting our own.
    count=[subviews count];
    for(i=0;i<count;i++)
     [(NSView *)[subviews objectAtIndex:i] _collectTrackingAreasForWindowInto:collector];
   }
}

-(NSArray *)trackingAreas {
   return _trackingAreas;
}

-(void)_trackingAreasChanged {
   [[self window] _invalidateTrackingAreas];
}

-(void)addTrackingArea:(NSTrackingArea *)trackingArea {
   [_trackingAreas addObject:trackingArea];
   [self _trackingAreasChanged];
}

-(void)removeTrackingArea:(NSTrackingArea *)trackingArea {
   [_trackingAreas removeObjectIdenticalTo:trackingArea];
   [self _trackingAreasChanged];
}

-(void)updateTrackingAreas {
   [self _trackingAreasChanged];
}

-(NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:owner userData:(void *)userData assumeInside:(BOOL)assumeInside {
   NSTrackingAreaOptions options=NSTrackingMouseEnteredAndExited|NSTrackingActiveAlways;
   NSTrackingArea *area=nil;

   if(assumeInside==YES)
    options|=NSTrackingAssumeInside;

   area=[[NSTrackingArea alloc] _initWithRect:rect options:options owner:owner userData:NULL retainUserData:NO isToolTip:NO isLegacy:NO];
   [_trackingAreas addObject:area];
   [area release];

   [self _trackingAreasChanged];

   /* NSTrackingRectTag is an integer in the API; hand back the area's index
    * and look it back up on removal. */
   return (NSTrackingRectTag)[_trackingAreas indexOfObjectIdenticalTo:area];
}

-(void)removeTrackingRect:(NSTrackingRectTag)tag {
   if(tag<0 || (NSUInteger)tag>=[_trackingAreas count])
    return;

   [self removeTrackingArea:[_trackingAreas objectAtIndex:(NSUInteger)tag]];
}

// a text input client gets one context for its lifetime, other views have none, as on macOS
-(NSTextInputContext *)inputContext {
   static char key;
   NSTextInputContext *context;

   if(![self respondsToSelector:@selector(insertText:replacementRange:)])
    return nil;
   context=objc_getAssociatedObject(self,&key);
   if(context==nil){
    context=[[[NSTextInputContext alloc] initWithClient:(id<NSTextInputClient>)self] autorelease];
    objc_setAssociatedObject(self,&key,context,OBJC_ASSOCIATION_RETAIN);
   }
   return context;
}

-(void)registerForDraggedTypes:(NSArray *)types {
   types=[types copy];
   [_draggedTypes release];
   _draggedTypes=types;
}

-(void)unregisterDraggedTypes {
   [_draggedTypes release];
   _draggedTypes=nil;
}

-(NSArray *)registeredDraggedTypes {
   return _draggedTypes;
}

-(void)_deepResignFirstResponder {
   if([_window firstResponder]==self)
    [_window makeFirstResponder:nil];

   [[self subviews] makeObjectsPerformSelector:_cmd];
}

-(void)removeFromSuperview {
   [_superview setNeedsDisplayInRect:[self frame]];
   [self removeFromSuperviewWithoutNeedingDisplay];
}

-(void)_removeViewWithoutDisplay:(NSView *)view  {
   [_subviews removeObjectIdenticalTo:view];
}

-(void)removeFromSuperviewWithoutNeedingDisplay {
   NSView *removeFrom=_superview;
   NSWindow *window=[self window];

   [removeFrom willRemoveSubview:self];
   [self _deepResignFirstResponder];
   [self _setSuperview:nil];
   [self _setWindow:nil];

   [removeFrom _removeViewWithoutDisplay:self];
   [window _invalidateTrackingAreas];
}

-(void)viewWillMoveToSuperview:(NSView *)view {
   // Intentionally empty.
}

-(void)viewDidMoveToSuperview {
   // Intentionally empty.
}

-(void)viewWillMoveToWindow:(NSWindow *)window {
   // Intentionally empty.
}

-(void)viewDidMoveToWindow {
   // Default implementation does nothing
}

-(BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event {
   return NO;
}

-(void)resizeSubviewsWithOldSize:(NSSize)oldSize {
   int i,count=[_subviews count];

   for(i=0;i<count;i++)
    [[_subviews objectAtIndex:i] resizeWithOldSuperviewSize:oldSize];
   [self _pdLayoutConstraints];
}

-(void)resizeWithOldSuperviewSize:(NSSize)oldSize {
   NSRect superFrame=[_superview frame];
   NSRect frame=[self frame];
   BOOL   originChanged=NO,sizeChanged=NO;

   if(_autoresizingMask&NSViewMinXMargin){
    if(_autoresizingMask&NSViewWidthSizable){
     if(_autoresizingMask&NSViewMaxXMargin){
      frame.origin.x+=((superFrame.size.width-oldSize.width)/3);
      frame.size.width+=((superFrame.size.width-oldSize.width)/3);
     }
     else {
      frame.origin.x+=((superFrame.size.width-oldSize.width)/2);
      frame.size.width+=((superFrame.size.width-oldSize.width)/2);
     }
     originChanged=YES;
     sizeChanged=YES;
    }
    else if(_autoresizingMask&NSViewMaxXMargin){
     frame.origin.x+=((superFrame.size.width-oldSize.width)/2);
     originChanged=YES;
    }
    else{
     frame.origin.x+=superFrame.size.width-oldSize.width;
     originChanged=YES;
    }
   }
   else if(_autoresizingMask&NSViewWidthSizable){
    if(_autoresizingMask&NSViewMaxXMargin)
     frame.size.width+=((superFrame.size.width-oldSize.width)/2);
    else
     frame.size.width+=superFrame.size.width-oldSize.width;
    sizeChanged=YES;
   }
   else if(_autoresizingMask&NSViewMaxXMargin){
    // don't move or resize
   }


   if(_autoresizingMask& NSViewMinYMargin){
    if(_autoresizingMask& NSViewHeightSizable){
     if(_autoresizingMask& NSViewMaxYMargin){
      frame.origin.y+=((superFrame.size.height-oldSize.height)/3);
      frame.size.height+=((superFrame.size.height-oldSize.height)/3);
     }
     else {
      frame.origin.y+=((superFrame.size.height-oldSize.height)/2);
      frame.size.height+=((superFrame.size.height-oldSize.height)/2);
     }
     originChanged=YES;
     sizeChanged=YES;
    }
    else if(_autoresizingMask& NSViewMaxYMargin){
     frame.origin.y+=((superFrame.size.height-oldSize.height)/2);
     originChanged=YES;
    }
    else {
     frame.origin.y+=superFrame.size.height-oldSize.height;
     originChanged=YES;
    }
   }
   else if(_autoresizingMask&NSViewHeightSizable){
    if(_autoresizingMask& NSViewMaxYMargin)
     frame.size.height+=((superFrame.size.height-oldSize.height)/2);
    else
     frame.size.height+=superFrame.size.height-oldSize.height;
    sizeChanged=YES;
   }

   if(originChanged || sizeChanged)
    [self setFrame:frame];
}

-(BOOL)inLiveResize {
   return _inLiveResize;
}

-(BOOL)preservesContentDuringLiveResize {
   return NO;
}

-(NSRect)rectPreservedDuringLiveResize {
   return NSZeroRect;
}

-(void)viewWillStartLiveResize {
    _inLiveResize=YES;
   [_subviews makeObjectsPerformSelector:_cmd];
}

-(void)viewDidEndLiveResize {
   _inLiveResize=NO;
   [_subviews makeObjectsPerformSelector:_cmd];
}

-(BOOL)enterFullScreenMode:(NSScreen *)screen withOptions:(NSDictionary *)options {
   NSWindow *window=[self window];

   if(window==nil)
    return NO;
   if(!([window styleMask]&NSFullScreenWindowMask))
    [window toggleFullScreen:self];
   return YES;
}

-(BOOL)isInFullScreenMode {
// dont issue warning
   return NO;
}

-(void)exitFullScreenModeWithOptions:(NSDictionary *)options {
   NSWindow *window=[self window];

   if([window styleMask]&NSFullScreenWindowMask)
    [window toggleFullScreen:self];
}

-(NSClipView *)_enclosingClipView {
   id result=[self superview];

   for(;result!=nil;result=[result superview])
    if([result isKindOfClass:[NSClipView class]])
     return result;

   return nil;
}

-(void)scrollPoint:(NSPoint)point {
   NSClipView *clipView=[self _enclosingClipView];

   if(clipView!=nil){
    NSPoint origin=[self convertPoint:point toView:clipView];

    [clipView scrollToPoint:origin];
   }
}

-(BOOL)scrollRectToVisible:(NSRect)rect {
    NSClipView *clipView = [self _enclosingClipView];
    NSView *documentView = [clipView documentView];
    // Current the document view visible rect in document view space
    NSRect vRect = [clipView documentVisibleRect];
    // Convert what we want in the document view space
    rect = [documentView convertRect:rect fromView:self];

    // Do the minimal amount of scrolling to show the rect

    // Missing amount on the four directions
    float missingLeft = NSMinX(vRect) - NSMinX(rect);
    float missingRight = NSMaxX(rect) - NSMaxX(vRect);

    float missingTop = NSMinY(vRect) - NSMinY(rect);
    float missingBottom = NSMaxY(rect) - NSMaxY(vRect);

    float dx = 0.;
    float dy = 0.;

    if (missingLeft * missingRight < 0) {
        // We need to scroll in one direction - no need to scroll if we're missing bits both ways or
        // if everything is visible

        // Let's do the minimal amount of scrolling
        if (fabs(missingLeft) < fabs(missingRight)) {
            dx = -missingLeft;
        } else {
            dx = missingRight;
        }
    }

    if (missingTop * missingBottom < 0) {
        // We need to scroll in one direction - no need to scroll if we're missing bits both ways or
        // if everything is visible

        // Let's do the minimal amount of scrolling
        if (fabs(missingTop) < fabs(missingBottom)) {
            dy = -missingTop;
        } else {
            dy = missingBottom;
        }
    }
    if (dx != 0 || dy != 0) {
        NSPoint pt = vRect.origin;
        pt.x += dx;
        pt.y += dy;
        pt = [documentView convertPoint:pt toView:clipView];
        [clipView scrollToPoint:pt];
        return YES;
    }
    return NO;
}

-(void)scrollClipView:(NSClipView *)clipView toPoint:(NSPoint)newOrigin {
   [clipView scrollToPoint:newOrigin];
}

-(BOOL)mouse:(NSPoint)point inRect:(NSRect)rect {
   return NSMouseInRect(point, rect, [self isFlipped]);
}

-(void)reflectScrolledClipView:(NSClipView *)view {
   [_superview reflectScrolledClipView:view];
}

-(void)allocateGState {
   // unimplemented
}

-(void)releaseGState {
   // unimplemented
}

-(void)setUpGState {
   // do nothing
}

-(void)renewGState {
   // do nothing
}

-(BOOL)wantsLayer {
   return _wantsLayer;
}

-(CALayer *)layer {
   return _layer;
}

-(CALayer *)makeBackingLayer {
   return [NSViewBackingLayer layer];
}

// the backing store's pixels: points times the window's scale, origin unchanged
static CGFloat backingScale(NSView *self){
   NSWindow *window=[self window];

   return window!=nil ? [window backingScaleFactor] : 1.0;
}

-(NSSize)convertSizeToBacking:(NSSize)size {
   CGFloat s=backingScale(self);

   return NSMakeSize(size.width*s,size.height*s);
}

-(NSSize)convertSizeFromBacking:(NSSize)size {
   CGFloat s=backingScale(self);

   return NSMakeSize(size.width/s,size.height/s);
}

-(NSPoint)convertPointToBacking:(NSPoint)point {
   CGFloat s=backingScale(self);

   return NSMakePoint(point.x*s,point.y*s);
}

-(NSPoint)convertPointFromBacking:(NSPoint)point {
   CGFloat s=backingScale(self);

   return NSMakePoint(point.x/s,point.y/s);
}

-(NSRect)convertRectToBacking:(NSRect)rect {
   CGFloat s=backingScale(self);

   return NSMakeRect(rect.origin.x*s,rect.origin.y*s,rect.size.width*s,rect.size.height*s);
}

-(NSRect)convertRectFromBacking:(NSRect)rect {
   CGFloat s=backingScale(self);

   return NSMakeRect(rect.origin.x/s,rect.origin.y/s,rect.size.width/s,rect.size.height/s);
}

-(void)_removeLayerFromSuperlayer {
   [_layer removeFromSuperlayer];
   [_layerContext invalidate];
   [_layerContext release];
   _layerContext=nil;
}

-(void)_createLayerContextIfNeeded {
   if([_superview layer]==nil){
    _layerContext=[[CALayerContext alloc] initWithFrame:[self frame]];
    [_layerContext setDelegate:self];
    [_layerContext setLayer:_layer];
   }
}

// only the layer tree changed, so recomposite it over what the views last drew: as with CoreAnimation
// a sublayer or contents change does not run -drawRect: (Gecko commits ~60 frames a second)
-(void)layerContextNeedsDisplay:(CALayerContext *)context {
   if(![self canDraw]){
    [self setNeedsDisplay:YES];
    return;
   }
   [self _drawLayerContext];
   [[self window] flushWindow];
}

// the layer tree renders offscreen, its pixels go into the window over what the views drew
-(void)_drawLayerContext {
   CGImageRef image;


   if(_layerContext==nil || ![self canDraw])
    return;
   // a bare backing layer has nothing of its own to show, so skip the readback
   if([[_layer sublayers] count]==0 && [_layer contents]==nil && [_layer backgroundColor]==NULL && [_layer borderWidth]==0)
    return;
   [_layerContext render];
   if((image=[_layerContext createImage])==NULL)
    return;
   // a view with a layer draws into a bitmap of its own, so this goes to the window's context directly
   NSGraphicsContext *windowContext=[[self window] graphicsContext];
   CGContextRef port=[windowContext graphicsPort];
   NSRect bounds=[self bounds];

   if(port==NULL){
    CGImageRelease(image);
    return;
   }
   CGContextSaveGState(port);
   CGContextResetClip(port);
   CGContextSetCTM(port,[self transformToWindow]);
   CGContextClipToRect(port,[self visibleRect]);
   if([self isFlipped]){
    CGContextTranslateCTM(port,0,NSMaxY(bounds)+NSMinY(bounds));
    CGContextScaleCTM(port,1,-1);
   }
   CGContextDrawImage(port,bounds,image);
   CGContextRestoreGState(port);
   CGImageRelease(image);
}

-(void)_addLayerToSuperlayer {
   if(_layer==nil)
    return;
   [[_superview layer] addSublayer:_layer];
   [self _createLayerContextIfNeeded];
}

/*
  When turning off layering, we only turn off layers below us which did not want a layer
  Layers which did want a layer are not touched, nor are their children.
 */
-(void)_removeLayerBackedViewsFromTree {
   if(_wantsLayer)
    [self _createLayerContextIfNeeded];
   else {
    [self _removeLayerFromSuperlayer];

// A backing layer is removed regardless of whether it was set implicitly or explicitly
// The distinction appears to be based on the layers class, not how it was set (host vs. backing)
    if([_layer isKindOfClass:[NSViewBackingLayer class]]){
     [_layer release];
     _layer=nil;
    }

    [_subviews makeObjectsPerformSelector:_cmd];
   }
}

-(void)_createLayersInTreeIfNeeded {
   // a backing CAMetalLayer (makeBackingLayer overridden, as Metal and Vulkan views do) goes to its own
   // subsurface, which needs no CALayer renderer
   if(!NSViewLayersEnabled){
    // a view that asks for a layer gets one as on macOS: apps take -layer as the root of their own tree
    // (Firefox adds its tiles under it), so it renders like a hosted layer
    CALayer *layer=_layer==nil ? [self makeBackingLayer] : nil;

    if(layer!=nil && (_wantsLayer || [layer isKindOfClass:[CAMetalLayer class]])){
     [self setLayer:layer];
     // the view is its backing layer's delegate, and the layer is sized like the view from the start:
     // Vulkan reads the surface's extent before the window shows
     [layer setDelegate:self];
     configureLayerGeometry(self);
    }
    [_subviews makeObjectsPerformSelector:_cmd];
    return;
   }

   if(_layer==nil){
    _layer=[[self makeBackingLayer] retain];
    configureLayerGeometry(self);
   }

   [self _addLayerToSuperlayer];

   [_subviews makeObjectsPerformSelector:_cmd];
}

-(void)setWantsLayer:(BOOL)value {
   if(getenv("PD_CA_TRACE"))
    fprintf(stderr,"PD-CA view %s %p setWantsLayer: %d\n",object_getClassName(self),self,value);
   if(!value){
    if(_wantsLayer){
     _wantsLayer=NO;
     [self _removeLayerBackedViewsFromTree];
    }
   }
   else {
    if(!_wantsLayer){
     _wantsLayer=YES;
     [self _createLayersInTreeIfNeeded];
    }
   }
}

-(void)setLayer:(CALayer *)value {
   if(getenv("PD_CA_TRACE"))
    fprintf(stderr,"PD-CA view %s %p setLayer: %s %p\n",object_getClassName(self),self,value ? object_getClassName(value) : "nil",value);
   // a hosted CAMetalLayer goes to its own subsurface, outside the CALayer renderer (which may be off)
   if([value isKindOfClass:[CAMetalLayer class]] || [_layer isKindOfClass:[CAMetalLayer class]]){
    if(value==_layer)
     return;
    if([_layer isKindOfClass:[CAMetalLayer class]])
     [(CAMetalLayer *)_layer _pdDetach];
    [value retain];
    [_layer release];
    _layer=value;
    updateMetalLayer(self);
    return;
   }

   // a layer the app hosts always renders, the NSViewLayersEnabled default only governs backing layers
   if(value!=_layer){
    [_subviews makeObjectsPerformSelector:@selector(_removeLayerFromSuperlayer)];

    value=[value retain];

    if(_layer==nil){
     if(value!=nil){
      _layer=value;
      configureLayerGeometry(self);
      [self _addLayerToSuperlayer];
     }
    }
    else if(value==nil){
     [self _removeLayerFromSuperlayer];
     [_layer release];
     _layer=nil;
    }
    else {
     [[_superview layer] replaceSublayer:_layer with:value];
     [_layer release];
      _layer=value;
    }

    [_subviews makeObjectsPerformSelector:@selector(_addLayerToSuperlayer)];
   }
}

// macOS 14: a display link for the screen the view is on (one 60 Hz timer here)
-(CADisplayLink *)displayLinkWithTarget:(id)target selector:(SEL)selector {
   return [CADisplayLink displayLinkWithTarget:target selector:selector];
}

-(NSViewLayerContentsPlacement)layerContentsPlacement {
   return _layerContentsPlacement;
}

-(void)setLayerContentsPlacement:(NSViewLayerContentsPlacement)value {
   _layerContentsPlacement=value;
}

-(NSViewLayerContentsRedrawPolicy)layerContentsRedrawPolicy {
   return _layerContentsRedrawPolicy;
}

-(void)setLayerContentsRedrawPolicy:(NSViewLayerContentsRedrawPolicy)value {
   _layerContentsRedrawPolicy=value;
}


-(NSArray *)backgroundFilters {
   return objc_getAssociatedObject(self,@selector(backgroundFilters))?:[NSArray array];
}

-(void)setBackgroundFilters:(NSArray *)filters {
   objc_setAssociatedObject(self,@selector(backgroundFilters),filters,OBJC_ASSOCIATION_COPY);
}

-(NSArray *)contentFilters {
   return _contentFilters;
}

-(void)setContentFilters:(NSArray *)filters {
   filters=[filters copy];
   [_contentFilters release];
   _contentFilters=filters;
}

-(CIFilter *)compositingFilter {
   return _compositingFilter;
}

-(void)setCompositingFilter:(CIFilter *)filter {
   filter=[filter copy];
   [_compositingFilter release];
   _compositingFilter=filter;
}

-(NSShadow *)shadow {
   return _shadow;
}

-(void)setShadow:(NSShadow *)shadow {
   shadow=[shadow copy];
   [_shadow release];
   _shadow=shadow;
}

-(BOOL)needsDisplay {
   return _needsDisplay;
}

/*
   If _needsDisplay is YES and there are no _invalidRects, invalid rect is bounds
   If _needsDisplay is YES and there are _invalidRects, invalid rect is union
   You can't just keep a running invalid rect because setting YES then changing the
   bounds should redraw the new bounds, but changing the bounds should not alter the
   invalidated rects.
 */

 static NSRect unionOfInvalidRects(NSView *self){
   NSRect result;

   if(self->_invalidRectCount==0)
    result=[self visibleRect];
   else {
    int i;

    result=self->_invalidRects[0];

    for(i=1;i<self->_invalidRectCount;i++)
     result=NSUnionRect(result,self->_invalidRects[i]);
   }

   return result;
}

static void removeRectFromInvalidInVisibleRect(NSView *self,NSRect rect,NSRect visibleRect) {
   int count=self->_invalidRectCount;

   while(--count>=0){
    self->_invalidRects[count]=NSIntersectionRect(self->_invalidRects[count],visibleRect);

    if(NSContainsRect(rect,self->_invalidRects[count])){
     int i;

     self->_invalidRectCount--;
     for(i=count;i<self->_invalidRectCount;i++)
      self->_invalidRects[i]=self->_invalidRects[i+1];
    }
   }
    if(self->_invalidRectCount==0){
        if(self->_invalidRects!=NULL) {
            NSZoneFree(NULL,self->_invalidRects);
            self->_invalidRects=NULL;
            // We killed the last invalidRect - we're clean now
            self->_needsDisplay=NO;
        } else if (NSContainsRect(rect, visibleRect)) {
            // We had no invalidRect, which means the full visibleRect was dirty
            // We're now clean
            self->_needsDisplay=NO;
        }
    }
}

static void clearRectsBeingRedrawn(NSView *self){
    if (self->_rectsBeingRedrawn) {
        NSZoneFree(NULL, self->_rectsBeingRedrawn);
        self->_rectsBeingRedrawn = NULL;
        self->_rectsBeingRedrawnCount = 0;
    }
}

static void clearInvalidRects(NSView *self){
   if(self->_invalidRects!=NULL)
    NSZoneFree(NULL,self->_invalidRects);
   self->_invalidRects=NULL;
   self->_invalidRectCount=0;
    clearRectsBeingRedrawn(self);

}

static void clearNeedsDisplay(NSView *self){
    if ([NSGraphicsContext inQuartzDebugMode]) {
        return;
    }
   clearInvalidRects(self);
   self->_needsDisplay=NO;
}

-(void)setNeedsDisplay:(BOOL)flag {
   _needsDisplay=flag;

// We removed them for YES to indicate entire view, and NO for obvious reasons
   clearInvalidRects(self);

   if(_needsDisplay)
    [[self window] setViewsNeedDisplay:YES];
}

-(void)setNeedsDisplayInRect:(NSRect)rect {
// We only add rects if its not the entire view
   if(!_needsDisplay || _invalidRects!=NULL){
       NSRect visibleRect = [self visibleRect];
       rect = NSIntersectionRect(visibleRect, rect);
       if (NSContainsRect(rect, visibleRect)) {
           clearInvalidRects(self); // Everything is dirty
       } else {
           // All of our clipping done by the context is rounded - so we need to do the same
           // here else we might get some artifact on clipping borders
           rect = [self convertRect:rect toView:nil];
           rect = NSIntegralRect(rect);
           rect = [self convertRect:rect fromView:nil];
           _invalidRectCount++;
           _invalidRects=NSZoneRealloc(NULL,_invalidRects,sizeof(NSRect)*_invalidRectCount);
           _invalidRects[_invalidRectCount-1]=rect;
       }
       clearRectsBeingRedrawn(self);

       // We also needs to be sure all of our superviews will properly redraw this area,
       // even if they are smart about what to redraw (using needsDisplayInRect:)
       NSView *opaqueAncestor = [self opaqueAncestor];
       if (opaqueAncestor != self) {
            NSRect dirtyRect = [self convertRect:rect toView:opaqueAncestor];
            [opaqueAncestor setNeedsDisplayInRect:dirtyRect];
       }
   }

   _needsDisplay=YES;
   [[self window] setViewsNeedDisplay:YES];
}

-(void)setKeyboardFocusRingNeedsDisplayInRect:(NSRect)rect {
   [self setNeedsDisplayInRect:NSInsetRect(rect,-4,-4)];
}

-(void)translateRectsNeedingDisplayInRect:(NSRect)rect by:(NSSize)delta {
   NSRect moved=NSOffsetRect(rect,delta.width,delta.height);

   [self setNeedsDisplayInRect:NSIntersectionRect(NSUnionRect(rect,moved),[self bounds])];
}

-(BOOL)canDraw {
   return _window!=nil && ![self isHiddenOrHasHiddenAncestor];
}

-(BOOL)canDrawConcurrently {
   return [objc_getAssociatedObject(self,@selector(canDrawConcurrently)) boolValue];
}

-(BOOL)wantsUpdateLayer {
   return NO;
}

-(void)updateLayer {
   // for subclasses that set their layer's contents themselves
}

-(void)viewWillDraw {
    [_subviews makeObjectsPerformSelector:_cmd];
}

-(void)setCanDrawConcurrently:(BOOL)canDraw {
   objc_setAssociatedObject(self,@selector(canDrawConcurrently),[NSNumber numberWithBool:canDraw],OBJC_ASSOCIATION_RETAIN);
}

// PD_VIEW_TRACE=1: which views get a backing bitmap per display, tallied every 200 displays
static void traceBackingDisplay(NSView *view,NSRect frame){
   static int on=-1;
   static unsigned long count;
   static NSCountedSet *classes;

   if(on<0)
    on=getenv("PD_VIEW_TRACE")!=NULL;
   if(!on)
    return;
   if(classes==nil)
    classes=[[NSCountedSet alloc] init];
   [classes addObject:[NSString stringWithFormat:@"%s %gx%g layer %s superview %s",object_getClassName(view),frame.size.width,frame.size.height,
    object_getClassName(view->_layer),object_getClassName([view superview])]];
   if(++count%200==0){
    for(NSString *entry in classes)
     fprintf(stderr,"PD-VIEW %lu displays: %lu x %s\n",count,(unsigned long)[classes countForObject:entry],[entry UTF8String]);
   }
}

static NSGraphicsContext *graphicsContextForView(NSView *view){
   if(view->_layer!=nil){
    NSRect             frame=[view frame];

    traceBackingDisplay(view,frame);
    size_t             width=frame.size.width;
    size_t             height=frame.size.height;
    static char        backingKey;
    NSGraphicsContext *result=objc_getAssociatedObject(view,&backingKey);
    CGContextRef       port=[result graphicsPort];

    // one backing bitmap per view while its size holds, cleared for each display as on macOS,
    // instead of a window-size allocation per display
    if(port!=NULL && CGBitmapContextGetWidth(port)==width && CGBitmapContextGetHeight(port)==height){
     CGContextSaveGState(port);
     CGContextResetClip(port);
     CGContextSetCTM(port,CGAffineTransformIdentity);
     CGContextClearRect(port,CGRectMake(0,0,width,height));
     CGContextRestoreGState(port);
     return result;
    }

    CGColorSpaceRef    colorSpace=CGColorSpaceCreateDeviceRGB();
    CGContextRef       context=CGBitmapContextCreate(NULL,width,height,8,0,colorSpace,kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Host);

    result=[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
    objc_setAssociatedObject(view,&backingKey,result,OBJC_ASSOCIATION_RETAIN);
    CGColorSpaceRelease(colorSpace);
    CGContextRelease(context);

    return result;
   }

   return [[view window] graphicsContext];
}

-(void)_lockFocusInContext:(NSGraphicsContext *)context {
    CGContextRef graphicsPort=[context graphicsPort];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:context];
    [[context focusStack] addObject:self];
    CGContextSaveGState(graphicsPort);
    CGContextResetClip(graphicsPort);

    if(_layer!=nil)
     CGContextSetCTM(graphicsPort,[self transformToLayer]);
    else
     CGContextSetCTM(graphicsPort,[self transformToWindow]);

    CGContextClipToRect(graphicsPort,[self visibleRect]);

    [self setUpGState];
}

-(void)lockFocus {
   [self _lockFocusInContext:graphicsContextForView(self)];
}

-(BOOL)lockFocusIfCanDraw {
   if([self canDraw]){
    [self lockFocus];
    return YES;
   }
   return NO;
}

-(BOOL)lockFocusIfCanDrawInContext:(NSGraphicsContext *)context {
   if(context!=nil){
    [self _lockFocusInContext:context];
    return YES;
   }
   return NO;
}


-(void)unlockFocus {
   NSGraphicsContext *graphicsContext=[NSGraphicsContext currentContext];
   CGContextRef       context=[graphicsContext graphicsPort];

   // only a backing layer shows what the view drew, a hosted layer belongs to the app
   if([_layer isKindOfClass:[NSViewBackingLayer class]]){
    CGImageRef image=CGBitmapContextCreateImage(context);

    [(id)_layer _setContentsWithoutRender:(id)image];
    CGImageRelease(image);
   }

   CGContextRestoreGState(context);

   [[graphicsContext focusStack] removeLastObject];
   [NSGraphicsContext restoreGraphicsState];
}

-(BOOL)needsToDrawRect:(NSRect)rect {
    BOOL needsToDrawRect = NO;

    if (NSIntersectsRect(rect, _visibleRect)) {
        const NSRect *rects;
        NSUInteger count;
        [self getRectsBeingDrawn:&rects count:&count];
        if (count) {
            for(int i=0; i<count && needsToDrawRect == NO;i++) {
                needsToDrawRect = NSIntersectsRect(rect, rects[i]);
            }
        }
    }
    return needsToDrawRect;
}

-(void)getRectsBeingDrawn:(const NSRect **)rects count:(NSInteger *)count {
    // This method returns all the rects being drawn concerning the view
    // That's all of the dirty rects from the view, but also all the ones
    // from the superview that might have caused the redraw.
    // Since invalidating a rect also invalidates the first opaque superview,
    // only the opaque views need to be checked
    *rects = _rectsBeingRedrawn;
    *count = _rectsBeingRedrawnCount;

    if  (_rectsBeingRedrawn == NULL) {
        NSView *opaqueAncestor = [self opaqueAncestor];
        if (opaqueAncestor != self) {
            // Ask our opaque ancestor what to draw
            const NSRect *ancestorRects;
            [opaqueAncestor getRectsBeingDrawn:&ancestorRects count:&_rectsBeingRedrawnCount];
            if (_rectsBeingRedrawnCount) {
                _rectsBeingRedrawn = NSZoneCalloc(NULL, _rectsBeingRedrawnCount, sizeof(NSRect));
                int rectsCount = 0;
                for (int i = 0; i < _rectsBeingRedrawnCount; ++i) {
                    NSRect r = [opaqueAncestor convertRect:ancestorRects[i] toView:self];
                    // No need for the rects that are outside of the visibleRect
                    if (NSIntersectsRect(r, _visibleRect)) {
                        _rectsBeingRedrawn[rectsCount++] = r;
                    }
                }
                *rects = _rectsBeingRedrawn;
                *count = rectsCount;
            }
        } else {
            // We're opaque - concatenate our invalid rect with the one from the previous opaque view
            NSView *view = [self superview];
            if (view) {
                NSView *opaqueAncestor = [view opaqueAncestor];
                const NSRect *ancestorRects;
                NSUInteger ancestorRectsCount;
                [opaqueAncestor getRectsBeingDrawn:&ancestorRects count:&ancestorRectsCount];
                if (ancestorRectsCount || _invalidRectCount) {
                    _rectsBeingRedrawn = NSZoneCalloc(NULL, _invalidRectCount + ancestorRectsCount, sizeof(NSRect));
                    int rectsCount = 0;
                    for (int i = 0; i < ancestorRectsCount; ++i) {
                        NSRect r = [opaqueAncestor convertRect:ancestorRects[i] toView:self];
                        // No need for the rects that are outside of the visibleRect
                        if (NSIntersectsRect(r, _visibleRect)) {
                            _rectsBeingRedrawn[rectsCount++] = r;
                        }
                    }
                    for (int i = 0; i < _invalidRectCount; ++i) {
                        _rectsBeingRedrawn[rectsCount++] = _invalidRects[i];
                    }
                    _rectsBeingRedrawnCount = rectsCount;
                    *rects = _rectsBeingRedrawn;
                    *count = _rectsBeingRedrawnCount;
                }
            }
        }
    }
    // We had no info and no opaque ancestor gave us any useful rect - just use our invalid rects
    if (*rects == NULL) {
        if (_invalidRects == NULL) {
            if (_needsDisplay) {
                *rects = &_visibleRect;
                *count = 1;
            }
        } else {
            *rects = _invalidRects;
            *count = _invalidRectCount;
        }
    }
}

-(void)getRectsExposedDuringLiveResize:(NSRect)rects count:(NSInteger *)count {
   // nothing is preserved during a live resize, so the whole view is exposed
   if(count!=NULL)
    *count=1;
}

-(BOOL)shouldDrawColor {
   return YES;
}

-(NSView *)opaqueAncestor {
   if([self isOpaque])
    return self;

   return [[self superview] opaqueAncestor];
}

-(void)display {
   [self displayRect:[self visibleRect]];
}

- (NSEnumerator*)_subviewsInDisplayOrderEnumerator
{
    // Subviews are ordered back to front -
    return [_subviews objectEnumerator];
}

-(void)_displayIfNeededWithoutViewWillDraw {
   if([self needsDisplay]){
    [self displayRect:unionOfInvalidRects(self)];
    clearNeedsDisplay(self);
   }

    NSEnumerator* viewEnumerator = [self _subviewsInDisplayOrderEnumerator];

    NSView* subView = nil;
    while ((subView = [viewEnumerator nextObject])) {
        [subView _displayIfNeededWithoutViewWillDraw];
    }
}

-(void)displayIfNeeded {
   [self viewWillDraw];
   [self _displayIfNeededWithoutViewWillDraw];
}

-(void)displayIfNeededInRect:(NSRect)rect {

   rect=NSIntersectionRect(unionOfInvalidRects(self), rect);

   if([self needsDisplay])
    [self displayRect:rect];

    NSEnumerator* viewEnumerator = [self _subviewsInDisplayOrderEnumerator];

    NSView* child = nil;
    while ((child = [viewEnumerator nextObject])) {
        NSRect converted=NSIntersectionRect([self convertRect:rect toView:child],[child bounds]);
        if(!NSIsEmptyRect(converted)) {
         [child displayIfNeededInRect:converted];
        }
    }
}

-(void)displayIfNeededInRectIgnoringOpacity:(NSRect)rect {

   rect=NSIntersectionRect(unionOfInvalidRects(self), rect);

   if([self needsDisplay])
    [self displayRectIgnoringOpacity:rect];

    NSEnumerator* viewEnumerator = [self _subviewsInDisplayOrderEnumerator];

    NSView* child = nil;
    while ((child = [viewEnumerator nextObject])) {
        NSRect  converted=NSIntersectionRect([self convertRect:rect toView:child],[child bounds]);

        if(!NSIsEmptyRect(converted)) {
            [child displayIfNeededInRectIgnoringOpacity:converted];
        }
   }
}

-(void)displayIfNeededIgnoringOpacity {

   if([self needsDisplay])
    [self displayRectIgnoringOpacity:unionOfInvalidRects(self)];

    NSEnumerator* viewEnumerator = [self _subviewsInDisplayOrderEnumerator];

    NSView* child = nil;
    while ((child = [viewEnumerator nextObject])) {
        [child displayIfNeededIgnoringOpacity];
    }
}

-(void)displayRect:(NSRect)rect {
   NSView *opaque=[self opaqueAncestor];

   if(opaque!=self)
    rect=[self convertRect:rect toView:opaque];
   [opaque displayRectIgnoringOpacity:rect];
}

-(void)displayRectIgnoringOpacity:(NSRect)rect {
   NSRect visibleRect=[self visibleRect];

   rect=NSIntersectionRect(rect,visibleRect);

   if(NSIsEmptyRect(rect))
    return;

   if ([self canDraw] && _layer!=nil && [self wantsUpdateLayer] && ![NSGraphicsContext inQuartzDebugMode]) {
      // as on macOS a view that updates its layer itself gets no drawing context: no backing bitmap
      // per display (4 MB at window size), no snapshot into the layer
      [self updateLayer];

       NSEnumerator *viewEnumerator = [self _subviewsInDisplayOrderEnumerator];
       NSView *child = nil;

       while ((child = [viewEnumerator nextObject])) {
           NSRect check=NSIntersectionRect([self convertRect:rect toView:child],[child bounds]);

           if(!NSIsEmptyRect(check))
               [child displayRectIgnoringOpacity:check];
       }
   }
   else if ([self canDraw]) {
      // This view must be locked/unlocked prior to drawing subviews otherwise gState changes may affect subviews.
      [self lockFocus];

      NSGraphicsContext *context=[NSGraphicsContext currentContext];
      CGContextRef       graphicsPort=[context graphicsPort];

       CGContextClipToRect(graphicsPort,rect);

       const NSRect *rects;
       NSUInteger rectsCount;
       [self getRectsBeingDrawn:&rects count:&rectsCount];
       // If there is only one rect, it's the visible rect - it's already clipped
       if (rectsCount > 1)
           CGContextClipToRects(graphicsPort, rects, rectsCount);

      // [_window dirtyRect:[self convertRect:rect toView:nil]];
       if ([NSGraphicsContext inQuartzDebugMode]) {
           [[NSColor yellowColor] set];
           NSRectFill(rect);
       }
      else
           _NSAppearanceDrawView(self, ^{ [self drawRect:rect]; });

      [self unlockFocus];

       NSEnumerator *viewEnumerator = [self _subviewsInDisplayOrderEnumerator];

       NSView *child = nil;
       while ((child = [viewEnumerator nextObject])) {
           NSRect check=[self convertRect:rect toView:child];

           check=NSIntersectionRect(check,[child bounds]);

           if(!NSIsEmptyRect(check))
               [child displayRectIgnoringOpacity:check];
       }
   }

   [self _drawLayerContext];

    // Don't do anything to interfere with what will be drawn in non-debug mode
    if ([NSGraphicsContext inQuartzDebugMode] == NO) {
        removeRectFromInvalidInVisibleRect(self,rect,visibleRect);

        // Rects being drawn are only valid while we redraw
        clearRectsBeingRedrawn(self);
    }

   // We do the flushWindow here. If any of the display* methods are being used, you want it to update on screen immediately.
   // If the view hierarchy is being displayed as needed at the end of an event, flushing will be disabled and this will just
   // mark the window as needing flushing which will happen when all the views have finished being displayed
   [[self window] flushWindow];
}

-(void)displayRectIgnoringOpacity:(NSRect)rect inContext:(NSGraphicsContext *)context {
   NSGraphicsContext *saved=[NSGraphicsContext currentContext];

   rect=NSIntersectionRect(rect,[self bounds]);
   if(NSIsEmptyRect(rect) || context==nil)
    return;
   [NSGraphicsContext saveGraphicsState];
   [NSGraphicsContext setCurrentContext:context];
   CGContextRef port=[context graphicsPort];

   CGContextSaveGState(port);
   CGContextTranslateCTM(port,-NSMinX(rect),-NSMinY(rect));
   if([self isFlipped]){
    CGContextTranslateCTM(port,0,NSMaxY(rect)+NSMinY(rect));
    CGContextScaleCTM(port,1,-1);
   }
   CGContextClipToRect(port,rect);
   [self drawRect:rect];
   for(NSView *child in [self subviews]){
    NSRect check=NSIntersectionRect([self convertRect:rect toView:child],[child bounds]);

    if(NSIsEmptyRect(check))
     continue;
    CGContextSaveGState(port);
    NSRect frame=[child frame];

    CGContextTranslateCTM(port,NSMinX(frame),NSMinY(frame));
    [child displayRectIgnoringOpacity:check inContext:context];
    CGContextRestoreGState(port);
   }
   CGContextRestoreGState(port);
   [NSGraphicsContext restoreGraphicsState];
   [NSGraphicsContext setCurrentContext:saved];
}

-(void)drawRect:(NSRect)rect {
   // do nothing
}

-(BOOL)autoscroll:(NSEvent *)event {
   return [[self superview] autoscroll:event];
}

-(void)scrollRect:(NSRect)rect by:(NSSize)delta {
   NSPoint point=rect.origin;

   point.x+=delta.width;
   point.y+=delta.height;

   if([self lockFocusIfCanDraw]){
    NSCopyBits([self gState],rect,point);
    [self unlockFocus];
   }
}

-(BOOL)mouseDownCanMoveWindow {
   return ![self isOpaque];
}

-(void)print:sender {
   [[NSPrintOperation printOperationWithView:self] runOperation];
}

-(void)beginDocument {
}

-(void)endDocument {
}

-(void)beginPageInRect:(NSRect)rect atPlacement:(NSPoint)placement {
   CGContextRef      graphicsPort=NSCurrentGraphicsPort();
   CGRect            mediaBox=NSMakeRect(0,0,rect.size.width,rect.size.height);
   NSPrintInfo      *printInfo=[[NSPrintOperation currentOperation] printInfo];
   NSRect            imageableRect=[printInfo imageablePageBounds];
   CGAffineTransform transform=CGAffineTransformIdentity;

   [NSCurrentFocusStack() addObject:self];

    CGContextBeginPage(graphicsPort,&mediaBox);
    CGContextSaveGState(graphicsPort);

   transform=CGAffineTransformIdentity;
   if([self isFlipped]){
       transform=CGAffineTransformMake(1,0,0,-1,0,[printInfo paperSize].height);
   }

   transform=CGAffineTransformTranslate(transform,-rect.origin.x,-rect.origin.y);

   CGContextConcatCTM(graphicsPort,transform);

   [self setUpGState];
}

-(void)endPage {
   CGContextRef graphicsPort=NSCurrentGraphicsPort();

   CGContextRestoreGState(graphicsPort);
   CGContextEndPage(graphicsPort);
   [NSCurrentFocusStack() removeLastObject];
}

-(NSAttributedString *)pageHeader {
   NSString *title=[self printJobTitle];

   return title?[[[NSAttributedString alloc] initWithString:title] autorelease]:nil;
}

-(NSAttributedString *)pageFooter {
   NSInteger page=[[NSPrintOperation currentOperation] currentPage];
   NSString *text=[NSString stringWithFormat:@"%ld",(long)page];

   return [[[NSAttributedString alloc] initWithString:text] autorelease];
}

-(NSString *)printJobTitle {
   NSString *title=[[self window] title];

   return [title length]?title:[[NSProcessInfo processInfo] processName];
}

-(void)drawSheetBorderWithSize:(NSSize)size {
   // no sheet border on paper
}

-(void)drawPageBorderWithSize:(NSSize)size {
   // no page border on paper
}


-(CGFloat)widthAdjustLimit {
   return 0.5;
}

-(CGFloat)heightAdjustLimit {
   return 0.5;
}

-(void)adjustPageWidthNew:(CGFloat *)adjusted left:(CGFloat)left right:(CGFloat)right limit:(CGFloat)limit {
// FIX, give subviews a chance
}

-(void)adjustPageHeightNew:(CGFloat *)adjust top:(CGFloat)top bottom:(CGFloat)bottom limit:(CGFloat)limit {
// FIX, give subviews a chance
}

-(BOOL)knowsPageRange:(NSRange *)range {
   return NO;
}

-(NSPoint)locationOfPrintRect:(NSRect)rect {
   return NSZeroPoint;
}

-(NSRect)rectForPage:(int)page {
   return NSZeroRect;
}

-(NSData *)dataWithEPSInsideRect:(NSRect)rect {
   NSMutableData    *result=[NSMutableData data];
   NSPrintOperation *operation=[NSPrintOperation EPSOperationWithView:self insideRect:rect toData:result];

   [operation runOperation];

   return result;
}

-(NSData *)dataWithPDFInsideRect:(NSRect)rect {
   NSMutableData    *result=[NSMutableData data];
   NSPrintOperation *operation=[NSPrintOperation PDFOperationWithView:self insideRect:rect toData:result];

   [operation runOperation];

   return result;
}

-(void)writeEPSInsideRect:(NSRect)rect toPasteboard:(NSPasteboard *)pasteboard {
   NSData *data=[self dataWithEPSInsideRect:rect];

   [pasteboard declareTypes:[NSArray arrayWithObject:NSPostScriptPboardType] owner:nil];
   [pasteboard setData:data forType:NSPostScriptPboardType];
}

-(void)writePDFInsideRect:(NSRect)rect toPasteboard:(NSPasteboard *)pasteboard {
   NSData *data=[self dataWithPDFInsideRect:rect];

   [pasteboard declareTypes:[NSArray arrayWithObject:NSPDFPboardType] owner:nil];
   [pasteboard setData:data forType:NSPDFPboardType];
}

-(void)dragImage:(NSImage *)image at:(NSPoint)location offset:(NSSize)offset event:(NSEvent *)event pasteboard:(NSPasteboard *)pasteboard source:source slideBack:(BOOL)slideBack {
    location = [self convertPoint:location toView:nil];
   [[NSDraggingManager draggingManager] dragImage:image at:location offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

-(BOOL)dragFile:(NSString *)path fromRect:(NSRect)rect slideBack:(BOOL)slideBack event:(NSEvent *)event {
   NSPasteboard *pasteboard=[NSPasteboard pasteboardWithName:NSDragPboard];
   NSImage *image=[[NSWorkspace sharedWorkspace] iconForFile:path];

   if(path==nil)
    return NO;
   [pasteboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
   [pasteboard setPropertyList:[NSArray arrayWithObject:path] forType:NSFilenamesPboardType];
   [self dragImage:image at:rect.origin offset:NSZeroSize event:event pasteboard:pasteboard source:self slideBack:slideBack];
   return YES;
}

-(BOOL)acceptsFirstResponder {
   return NO;
}

-(void)scrollWheel:(NSEvent *)event {
    NSScrollView *scrollView=[self enclosingScrollView];

    if(scrollView==nil) {
        /* If we can't handle it, pass up responder chain, yep, it does this. */
        [super scrollWheel:event];
    }
    else {
        NSView *documentView=[scrollView documentView];
        NSRect bounds=[documentView bounds];
        NSRect visible=[documentView visibleRect];
        float  direction=[documentView isFlipped]?-1:1;

        visible.origin.x+=[event deltaX]*[scrollView horizontalLineScroll]*3;
        visible.origin.y+=[event deltaY]*direction*[scrollView verticalLineScroll]*3;

        // Something equivalent to this should be in scrollRectToVisible:
        if(visible.origin.y<bounds.origin.y)
            visible.origin.y=bounds.origin.y;
        if(visible.origin.x<bounds.origin.x)
            visible.origin.x=bounds.origin.x;
        if(NSMaxY(visible)>NSMaxY(bounds))
            visible.origin.y=NSMaxY(bounds)-visible.size.height;
        if(NSMaxX(visible)>NSMaxX(bounds))
            visible.origin.x=NSMaxX(bounds)-visible.size.width;

        [documentView scrollRectToVisible:visible];
    }
}

-(BOOL)performKeyEquivalent:(NSEvent *)event {
   int i,count=[_subviews count];

   for(i=0;i<count;i++){
    NSView *check=[_subviews objectAtIndex:i];

    if([check performKeyEquivalent:event])
     return YES;
   }
   return NO;
}

-(BOOL)performMnemonic:(NSString *)string {
   for(NSView *child in [self subviews])
    if([child performMnemonic:string])
     return YES;
   return NO;
}

-(void)setMenu:(NSMenu *)menu {
   menu=[menu retain];
   [_menu release];
   _menu=menu;
}

-(void)rightMouseDown:(NSEvent *)event {
   [NSMenu popUpContextMenu:[self menuForEvent:event] withEvent:event forView:self];
}


// as on macOS NSView has no NSDraggingDestination methods, the drag loop treats a missing one as its default
-(NSArray *)_draggedTypes {
   return _draggedTypes;
}

- (BOOL)dragPromisedFilesOfTypes:(NSArray *)types fromRect:(NSRect)rect source:(id)source slideBack:(BOOL)slideBack event:(NSEvent *)event {
   NSPasteboard *pasteboard=[NSPasteboard pasteboardWithName:NSDragPboard];

   if([types count]==0)
    return NO;
   [pasteboard declareTypes:[NSArray arrayWithObject:@"Apple files promise pasteboard type"] owner:nil];
   [pasteboard setPropertyList:types forType:@"Apple files promise pasteboard type"];
   [self dragImage:[[[NSImage alloc] initWithSize:rect.size] autorelease] at:rect.origin offset:NSZeroSize
           event:event pasteboard:pasteboard source:source slideBack:slideBack];
   return YES;
}

-(NSPoint)convertPointFromBase:(NSPoint)aPoint; {
   return aPoint;
}

-(NSPoint)convertPointToBase:(NSPoint)aPoint; {
   return aPoint;
}

-(NSSize)convertSizeFromBase:(NSSize)aSize {
   return aSize;
}

-(NSSize)convertSizeToBase:(NSSize)aSize {
   return aSize;
}

-(NSRect)convertRectFromBase:(NSRect)aRect {
   return aRect;
}

-(NSRect)convertRectToBase:(NSRect)aRect {
   return aRect;
}

-(void)showDefinitionForAttributedString:(NSAttributedString *)string atPoint:(NSPoint)origin {
   // no dictionary service
}

+defaultAnimationForKey:(NSString *)key {
   return nil;
}

-animator {
   // changes through the animator take effect at once
   return self;
}

-(NSDictionary *)animations {
   return _animations;
}

-animationForKey:(NSString *)key {
   return [_animations objectForKey:key];
}

-(void)setAnimations:(NSDictionary *)dictionary {
   dictionary=[dictionary copy];
   [_animations release];
   _animations=dictionary;
}

// Blocks aren't supported by the compiler yet.
//-(void)showDefinitionForAttributedString:(NSAttributedString *)string range:(NSRange)range options:(NSDictionary *)options baselineOriginProvider:(NSPoint (^)(NSRange adjustedRange))originProvider {
//   NSUnimplementedMethod();
//}

-(NSString *)description {
    return [NSString stringWithFormat:@"<%@[0x%lx] frame: %@>", [self class], self, NSStringFromRect(_frame)];
}


// there is no OpenGL surface scaling here, the setting is only remembered for the getter
-(BOOL)wantsBestResolutionOpenGLSurface {
   NSNumber *value=objc_getAssociatedObject(self,@selector(wantsBestResolutionOpenGLSurface));

   return [value boolValue];
}

-(void)setWantsBestResolutionOpenGLSurface:(BOOL)value {
   objc_setAssociatedObject(self,@selector(wantsBestResolutionOpenGLSurface),[NSNumber numberWithBool:value],OBJC_ASSOCIATION_RETAIN);
}
@end
