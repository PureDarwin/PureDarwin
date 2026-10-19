/*
 * Copyright (c) 2006-2007 Christopher J. W. Lloyd
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#import <AppKit/NSThemeFrame.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSAppearance.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSToolbarView.h>
#import <AppKit/NSAttributedString.h>
#import <Onyx2D/O2Context.h>
#import <AppKit/NSScreen.h>

@interface NSThemeFrame (TitlebarPrivate)
-(void)_titlebarDoubleClicked;
@end
#include <stdint.h>

// libSystem exports notify(3), the sdk ships no header for it
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_get_state(int token, uint64_t *state64);

extern void CGNativeBorderFrameWidthsForStyle(unsigned styleMask, CGFloat *top,
    CGFloat *left, CGFloat *bottom, CGFloat *right);

@interface NSThemeFrame (PDControls)
-(void)_drawControlsInContext:(O2Context *)context;
@end

// the traffic lights in their own view above a full-size content view, which would cover the frame's drawing
@interface NSThemeFrameControls : NSView
@end

@implementation NSThemeFrameControls

-(BOOL)isOpaque {
   return NO;
}

-(void)drawRect:(NSRect)rect {
   O2Context *context = [[[self window] graphicsContext] graphicsPort];
   NSRect frame = [self frame];

   O2ContextSaveGState(context);
   O2ContextTranslateCTM(context, -frame.origin.x, -frame.origin.y);
   [(NSThemeFrame *)[self superview] _drawControlsInContext:context];
   O2ContextRestoreGState(context);
}

// presses go to the frame, which knows the buttons and moves the window from the rest
-(void)mouseDown:(NSEvent *)event {
   [[self superview] mouseDown:event];
}

@end

// the title bar views apps reach for, empty since the frame draws the title bar itself
@interface NSTitlebarContainerView : NSView
@end

@interface NSTitlebarView : NSView
@end

@implementation NSTitlebarContainerView

-(BOOL)isOpaque {
   return NO;
}

// presses fall through to the frame unless an app put a control in here
-(NSView *)hitTest:(NSPoint)point {
   NSView *hit = [super hitTest:point];

   if(hit == self || [hit isKindOfClass:[NSTitlebarView class]])
      return nil;
   return hit;
}

@end

@implementation NSTitlebarView

-(BOOL)isOpaque {
   return NO;
}

@end

@implementation NSThemeFrame

// all measurements in pixels
const float NSWindowTitleHeight = 32;
// toolbar heights of macOS's unified and compact styles
const float NSWindowUnifiedTitleHeight = 52;
const float NSWindowCompactTitleHeight = 38;
const float NSWindowEdgePad = 2;
const float NSWindowCornerRadius = 8;

// Settings' window style, the same state the compositor reads, bit 0 means square corners
static BOOL _pdWindowCornersSquare(void) {
    static int token = -1;
    uint64_t state = 0;

    if(token == -1 && notify_register_check("org.puredarwin.WindowServer.windowstyle", &token) != 0)
        token = -1;
    if(token == -1 || notify_get_state(token, &state) != 0)
        return NO;
    return (state & 0x1) != 0;
}
const float NSWindowControlDiameter = 12;
const float NSWindowControlSpacing = 8;
const float NSWindowControlInset = 12;
const float NSWindowTitleFontSize = 13;

static O2Image *wsClose, *wsCloseHover, *wsMini, *wsMiniHover;
static O2Image *wsZoom, *wsZoomUp, *wsZoomDown;
static BOOL wsImagesTried;

enum { kControlGlyphNone = 0, kControlGlyphClose, kControlGlyphMini, kControlGlyphZoom };

// Fallback used when the traffic light images are unavailable.
static void drawWindowControl(O2Context *context, CGRect rect,
    CGFloat r, CGFloat g, CGFloat b, int glyph)
{
    CGFloat radius = rect.size.width / 2;
    CGFloat cx = rect.origin.x + radius;
    CGFloat cy = rect.origin.y + radius;

    O2ContextSetRGBFillColor(context, r, g, b, 1);
    O2ContextBeginPath(context);
    // O2ContextAddArc takes fmod of the angles, so a full 2*pi sweep degenerates.
    O2ContextAddEllipseInRect(context, rect);
    O2ContextFillPath(context);

    // a faint darker rim keeps the light visible on a pale bar
    O2ContextSetRGBStrokeColor(context, r * 0.8, g * 0.8, b * 0.8, 1);
    O2ContextSetLineWidth(context, 0.5);
    O2ContextBeginPath(context);
    O2ContextAddEllipseInRect(context, CGRectInset(rect, 0.25, 0.25));
    O2ContextStrokePath(context);

    if(glyph == kControlGlyphNone)
        return;

    CGFloat inset = radius * 0.42;
    O2ContextSetRGBStrokeColor(context, 0, 0, 0, 0.55);
    O2ContextSetLineWidth(context, 1.2);
    O2ContextBeginPath(context);
    switch(glyph) {
        case kControlGlyphClose:
            O2ContextMoveToPoint(context, cx - inset, cy - inset);
            O2ContextAddLineToPoint(context, cx + inset, cy + inset);
            O2ContextMoveToPoint(context, cx - inset, cy + inset);
            O2ContextAddLineToPoint(context, cx + inset, cy - inset);
            break;
        case kControlGlyphMini:
            O2ContextMoveToPoint(context, cx - inset, cy);
            O2ContextAddLineToPoint(context, cx + inset, cy);
            break;
        case kControlGlyphZoom:
            O2ContextMoveToPoint(context, cx - inset, cy);
            O2ContextAddLineToPoint(context, cx + inset, cy);
            O2ContextMoveToPoint(context, cx, cy - inset);
            O2ContextAddLineToPoint(context, cx, cy + inset);
            break;
    }
    O2ContextStrokePath(context);
}

-initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    pointer = NSZeroPoint;

    // use singleton traffic light images
    if(!wsImagesTried) {
        wsImagesTried = YES;
        NSString *basepath = @"/System/Library/CoreServices/WindowServer.app/Contents/Resources";
        NSString *path = [NSString stringWithFormat:@"%@/close.png", basepath];
        wsClose = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
        path = [NSString stringWithFormat:@"%@/closeHover.png", basepath];
        wsCloseHover = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
        path = [NSString stringWithFormat:@"%@/mini.png", basepath];
        wsMini = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
        path = [NSString stringWithFormat:@"%@/miniHover.png", basepath];
        wsMiniHover = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
        path = [NSString stringWithFormat:@"%@/zoom.png", basepath];
        wsZoom = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
        path = [NSString stringWithFormat:@"%@/zoomUpHover.png", basepath];
        wsZoomUp = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
        path = [NSString stringWithFormat:@"%@/zoomDownHover.png", basepath];
        wsZoomDown = [[[NSBitmapImageRep alloc] initWithContentsOfFile:path] CGImage];
    }
    return self;
}

-(BOOL)isOpaque {
   return YES;
}

-(NSView *)hitTest:(NSPoint)point {
    static BOOL inRect = NO;

    pointer = point;
    if(NSPointInRect(pointer, controls)) {
        if(!inRect) {
            [self setNeedsDisplay:YES]; // highlight controls if mouse has gone into them
            [_controlsView setNeedsDisplay:YES];
            inRect = YES;
        }
    } else if(inRect) { // we left the control zone
            [self setNeedsDisplay:YES];
            [_controlsView setNeedsDisplay:YES];
            inRect = NO;
    }

    return [super hitTest:point];
}

-(NSWindowBorderType)windowBorderType {
   return _borderType;
}

-(void)setWindowBorderType:(NSWindowBorderType)borderType {
   _borderType = borderType;
   [self setNeedsDisplay:YES];
}

// only a full-size content view can grow the title bar, elsewhere the native border fixes it
-(CGFloat)_titleHeight {
    NSWindow *window = [self window];

    if(!([window styleMask] & NSFullSizeContentViewWindowMask))
        return NSWindowTitleHeight;
    switch([window toolbarStyle]) {
        case NSWindowToolbarStyleUnified:
            return NSWindowUnifiedTitleHeight;
        case NSWindowToolbarStyleUnifiedCompact:
            return NSWindowCompactTitleHeight;
        default:
            return NSWindowTitleHeight;
    }
}

-(void)_layoutControls {
    CGRect button = NSMakeRect(NSWindowControlInset,
        _frame.size.height - [self _titleHeight] / 2 - NSWindowControlDiameter / 2,
        NSWindowControlDiameter, NSWindowControlDiameter);

    _closeButtonRect = button;
    button.origin.x += NSWindowControlSpacing + NSWindowControlDiameter;
    _miniButtonRect = button;
    button.origin.x += NSWindowControlSpacing + NSWindowControlDiameter;
    _zoomButtonRect = button;
    controls = NSMakeRect(_closeButtonRect.origin.x, _closeButtonRect.origin.y,
            2*NSWindowControlSpacing + 3*NSWindowControlDiameter, NSWindowControlDiameter);
}

-(void)_drawControlsInContext:(O2Context *)_context {
    BOOL hover = NSPointInRect(pointer, controls);
    BOOL active = [[self window] isKeyWindow];

    // inactive windows draw flat grey lights until the pointer is over them, as on macOS
    if(!active && !hover) {
        drawWindowControl(_context, _closeButtonRect, 0.80, 0.80, 0.80, kControlGlyphNone);
        drawWindowControl(_context, _miniButtonRect, 0.80, 0.80, 0.80, kControlGlyphNone);
        drawWindowControl(_context, _zoomButtonRect, 0.80, 0.80, 0.80, kControlGlyphNone);
    } else if(wsClose != nil) {
        if(hover) {
            [_context drawImage:wsCloseHover inRect:_closeButtonRect];
            [_context drawImage:wsMiniHover inRect:_miniButtonRect];
            [_context drawImage:wsZoomUp inRect:_zoomButtonRect];
        } else {
            [_context drawImage:wsClose inRect:_closeButtonRect];
            [_context drawImage:wsMini inRect:_miniButtonRect];
            [_context drawImage:wsZoom inRect:_zoomButtonRect];
        }
    } else {
        drawWindowControl(_context, _closeButtonRect, 0.99, 0.36, 0.34,
            hover ? kControlGlyphClose : kControlGlyphNone);
        drawWindowControl(_context, _miniButtonRect, 1.00, 0.74, 0.20,
            hover ? kControlGlyphMini : kControlGlyphNone);
        drawWindowControl(_context, _zoomButtonRect, 0.16, 0.79, 0.25,
            hover ? kControlGlyphZoom : kControlGlyphNone);
    }
    // a window without the action keeps that light flat grey even when key
    if(!([[self window] styleMask] & NSMiniaturizableWindowMask))
        drawWindowControl(_context, _miniButtonRect, 0.82, 0.82, 0.82, kControlGlyphNone);
    if(!([[self window] styleMask] & NSResizableWindowMask))
        drawWindowControl(_context, _zoomButtonRect, 0.82, 0.82, 0.82, kControlGlyphNone);
}

-(NSRect)_controlsOverlayFrame {
    return NSMakeRect(0, NSHeight([self bounds]) - [self _titleHeight],
        2 * NSWindowControlInset + 3 * NSWindowControlDiameter + 2 * NSWindowControlSpacing, [self _titleHeight]);
}

-(void)dealloc {
    [_titlebarContainerView release];
    [_controlsView release];
    [super dealloc];
}

-(NSRect)_titlebarContainerFrame {
    NSRect bounds = [self bounds];

    return NSMakeRect(0, NSMaxY(bounds) - [self _titleHeight], NSWidth(bounds), [self _titleHeight]);
}

-(NSView *)titlebarContainerView {
    if(_titlebarContainerView == nil) {
        NSView *titlebarView;

        _titlebarContainerView = [[NSTitlebarContainerView alloc] initWithFrame:[self _titlebarContainerFrame]];
        [_titlebarContainerView setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
        titlebarView = [[NSTitlebarView alloc] initWithFrame:[_titlebarContainerView bounds]];
        [titlebarView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [_titlebarContainerView addSubview:titlebarView];
        [titlebarView release];
        [self addSubview:_titlebarContainerView];
    }
    return _titlebarContainerView;
}

-(NSView *)titlebarView {
    return [[[self titlebarContainerView] subviews] firstObject];
}

// the overlay exists only with a full-size content view and always stays the frontmost subview
-(void)addSubview:(NSView *)view {
    [super addSubview:view];
    if(view == _controlsView || !([[self window] styleMask] & NSFullSizeContentViewWindowMask))
        return;
    if(_controlsView == nil) {
        _controlsView = [[NSThemeFrameControls alloc] initWithFrame:[self _controlsOverlayFrame]];
        [_controlsView setAutoresizingMask:NSViewMinYMargin];
    } else {
        [_controlsView retain];
        [_controlsView removeFromSuperview];
        [_controlsView autorelease];
    }
    [super addSubview:_controlsView];
}

-(void)drawRect:(NSRect)rect {
    [self _pdDrawFrameInContext:[[[self window] graphicsContext] graphicsPort]];
}

// the frame drawn into any context, so a frame for a window AppKit does not own can be rendered offscreen
-(void)_pdDrawFrameInContext:(O2Context *)_context {
    NSRect bounds = [NSWindow contentRectForFrameRect:[self bounds]
        styleMask:[[self window] styleMask]];
    float cheatSheet = 0;

    switch(_borderType){
        case NSNoBorder:
            break;

        case NSWindowToolTipBorderType:
            [[NSColor blackColor] setStroke];
            NSFrameRect(bounds);
            bounds = NSInsetRect(bounds, 1, 1);
            cheatSheet = 1;
            break;

        case NSWindowSheetBorderType:
            NSDrawButton(bounds,bounds);
            bounds = NSInsetRect(bounds, 2, 2);
            cheatSheet = 2;
            break;
    }

    NSWindow *window = [self window];
    CGFloat r, g, b, a;
    [[[self window] backgroundColor] getRed:&r green:&g blue:&b alpha:&a];
    O2ContextSetRGBStrokeColor(_context, r, g, b, a);
    O2ContextSetRGBFillColor(_context, r, g, b, a);
    O2ContextFillRect(_context, bounds);

    if([[self window] styleMask] == NSBorderlessWindowMask || ([[self window] styleMask] & NSFullScreenWindowMask))
        return;

    if([[self window] isSheet])
        bounds.size.height += cheatSheet;

    NSUInteger style = [window styleMask];
    BOOL fullSize = (style & NSFullSizeContentViewWindowMask) != 0;
    BOOL clearBar = [window titlebarAppearsTransparent];

    [self _layoutControls];
    // a transparent title bar keeps the window's background colour
    BOOL dark = [[self effectiveAppearance] _isDark];
    CGFloat bar = dark ? 0.22 : 0.886;

    if(clearBar)
        O2ContextSetRGBFillColor(_context, r, g, b, a);
    else
        O2ContextSetGrayFillColor(_context, bar, 1);
    O2ContextSetGrayStrokeColor(_context, bar, 1);

    // let's round these corners, with cubic quarter circles: the arc call wraps its angles and squared the top ones
    float radius = _pdWindowCornersSquare() ? 0 : NSWindowCornerRadius;
    CGFloat x0 = _frame.origin.x, y0 = _frame.origin.y, x1 = NSMaxX(_frame), y1 = NSMaxY(_frame);
    CGFloat k = radius * 0.4477;
    O2ContextBeginPath(_context);
    O2ContextMoveToPoint(_context, x0 + radius, y1);
    O2ContextAddLineToPoint(_context, x1 - radius, y1);
    O2ContextAddCurveToPoint(_context, x1 - k, y1, x1, y1 - k, x1, y1 - radius);
    O2ContextAddLineToPoint(_context, x1, y0 + radius);
    O2ContextAddCurveToPoint(_context, x1, y0 + k, x1 - k, y0, x1 - radius, y0);
    O2ContextAddLineToPoint(_context, x0 + radius, y0);
    O2ContextAddCurveToPoint(_context, x0 + k, y0, x0, y0 + k, x0, y0 + radius);
    O2ContextAddLineToPoint(_context, x0, y1 - radius);
    O2ContextAddCurveToPoint(_context, x0, y1 - k, x0 + k, y1, x0 + radius, y1);
    O2ContextClosePath(_context);
    O2ContextFillPath(_context);

    // hairline between the title bar and the content
    if(!clearBar) {
        O2ContextSetGrayFillColor(_context, dark ? 0.11 : 0.725, 1);
        O2ContextFillRect(_context, CGRectMake(0, _frame.size.height - [self _titleHeight], _frame.size.width, 1));
    }

    // over a full-size content view the overlay draws the lights instead
    if(!fullSize)
        [self _drawControlsInContext:_context];

    BOOL active = [window isKeyWindow];
    NSString *t = [window titleVisibility] == NSWindowTitleHidden ? nil : [window title];

    if(t) {
        NSFont *titleFont = [NSFont titleBarFontOfSize:NSWindowTitleFontSize];
        NSColor *titleColor = dark ? [NSColor colorWithCalibratedWhite:active ? 0.9 : 0.54 alpha:1]
                                    : [NSColor colorWithCalibratedWhite:active ? 0.16 : 0.6 alpha:1];
        NSDictionary *attrs = @{
            NSFontAttributeName : titleFont,
            NSForegroundColorAttributeName : titleColor
        };
        // centred, and cut with an ellipsis where it would reach the lights, mirrored on the right.
        // a window too narrow for that centres the title in the space right of the lights
        CGFloat left = NSMaxX(_zoomButtonRect) + 12;
        CGFloat room = _frame.size.width - 2 * left;
        CGFloat start = left;
        if (room < [t sizeWithAttributes:attrs].width) {
            CGFloat narrow = _frame.size.width - left - 8;
            if (narrow > room) {
                room = narrow;
                start = left;
            }
        } else
            start = (_frame.size.width - room) / 2;
        NSString *shown = t;
        if (room <= 0)
            shown = @"";
        else if ([shown sizeWithAttributes:attrs].width > room) {
            NSUInteger n = [t length];
            while (n > 0 && [[[t substringToIndex:n] stringByAppendingString:@"…"] sizeWithAttributes:attrs].width > room)
                n--;
            shown = n > 0 ? [[t substringToIndex:n] stringByAppendingString:@"…"] : @"";
        }
        NSAttributedString *title = [[NSAttributedString alloc] initWithString:shown attributes:attrs];
        NSSize size = [title size];
        // the typesetter puts the baseline ceil(ascender) below the rect top, and Inter's
        // glyphs still land 2px under the lights' centre line as measured on PD
        CGFloat mid = _frame.size.height - [self _titleHeight] / 2;
        CGFloat baseline = round(mid - [titleFont capHeight] / 2) + 2;
        CGFloat x = floor(_frame.size.width / 2 - size.width / 2);
        if (x < start)
            x = floor(start + (room - size.width) / 2);
        NSRect titleRect = NSMakeRect(
            x,
            baseline + ceil([titleFont ascender]) - size.height,
            size.width, size.height);
        [title drawInRect:titleRect];
    }
}

-(void)resizeSubviewsWithOldSize:(NSSize)oldSize {
   NSToolbarView *toolbarView=nil;
   NSView *contentView=nil;

   NSView *windowContent=[[self window] contentView];
   NSMutableArray *others=[NSMutableArray array];

   // the content is the subview holding the window's contentView, views apps add here keep their own masks
   for(NSView *view in _subviews){
    if(view == _controlsView || view == _titlebarContainerView)
     continue;
    if([view isKindOfClass:[NSToolbarView class]])
     toolbarView=(NSToolbarView *)view;
    else if(contentView==nil && windowContent!=nil && (view==windowContent || [windowContent isDescendantOf:view]))
     contentView=view;
    else
     [others addObject:view];
   }
   if(contentView==nil && windowContent==nil && [others count]>0){
    contentView=[others lastObject];
    [others removeLastObject];
   }

   NSRect toolbarFrame=(toolbarView!=nil)?[toolbarView frame]:NSZeroRect;
   NSRect contentFrame=[[[self window] class] contentRectForFrameRect:[self bounds] styleMask:[[self window] styleMask]];
   toolbarFrame.origin.y=NSMaxY(contentFrame)-toolbarFrame.size.height;
   toolbarFrame.origin.x=contentFrame.origin.x;
   toolbarFrame.size.width=contentFrame.size.width;

   [toolbarView setFrame:toolbarFrame];
   [toolbarView layoutViews];

   contentFrame.size.height-=toolbarFrame.size.height;
   [contentView setFrame:contentFrame];
   for(NSView *view in others)
    [view resizeWithOldSuperviewSize:oldSize];
   [_controlsView setFrame:[self _controlsOverlayFrame]];
   [_titlebarContainerView setFrame:[self _titlebarContainerFrame]];
}

-(void)mouseDown:(NSEvent *)event {
    CGFloat top, left, right, bottom;
    CGNativeBorderFrameWidthsForStyle([[self window] styleMask], &top, &left, &bottom, &right);
    NSPoint pos = [event locationInWindow];

    // no title bar or buttons in full screen
    if([[self window] styleMask] & NSFullScreenWindowMask)
        return;
    if([[self window] _resizeEdgesAtPoint:pos] != 0) {
        [[self window] requestResize:event];
        return;
    }

    if(([[self window] styleMask] & NSClosableWindowMask) &&
       NSPointInRect(pos, _closeButtonRect))
        [[self window] performClose:self];
    else if(([[self window] styleMask] & NSMiniaturizableWindowMask) &&
            NSPointInRect(pos, _miniButtonRect))
        [[self window] performMiniaturize:self];
    else if(([[self window] styleMask] & NSResizableWindowMask) &&
            NSPointInRect(pos, _zoomButtonRect))
        [[self window] performZoom:self];
    else if(pos.y > (NSMaxY(_frame) - top) && [event clickCount] == 2)
        [self _titlebarDoubleClicked];
    else if(pos.y > (NSMaxY(_frame) - top)) // in titlebar?
        [[self window] requestMove:event];
    else if([[self window] isMovableByWindowBackground])
        [[self window] requestMove:event];
}

// the private frame view methods macOS has and apps such as Firefox override to draw their own title bars
-(CGFloat)_titlebarHeight {
    return [self _titleHeight];
}

-(NSPoint)_closeButtonOrigin {
    return _closeButtonRect.origin;
}

-(void)_tileTitlebarAndRedisplay:(BOOL)redisplay {
    [self _layoutControls];
    [_controlsView setFrame:[self _controlsOverlayFrame]];
    if(redisplay)
        [self setNeedsDisplay:YES];
}

// AppleActionOnDoubleClick is Maximize (zoom, the default), Fill, Minimize or None
-(NSString *)_doubleClickAction {
    NSString *action = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleActionOnDoubleClick"];

    if(action == nil && [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleMiniaturizeOnDoubleClick"] != nil)
        action = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleMiniaturizeOnDoubleClick"] ? @"Minimize" : @"Maximize";
    return action ?: @"Maximize";
}

-(BOOL)_shouldZoomOnDoubleClick {
    return [[self _doubleClickAction] isEqualToString:@"Maximize"];
}

-(BOOL)_shouldFillOnDoubleClick {
    return [[self _doubleClickAction] isEqualToString:@"Fill"];
}

-(BOOL)_shouldMiniaturizeOnDoubleClick {
    return [[self _doubleClickAction] isEqualToString:@"Minimize"];
}

// fills the screen's visible area without entering full screen
-(void)_zoomFill:(id)sender {
    NSWindow *window = [self window];
    NSScreen *screen = [window screen] ?: [NSScreen mainScreen];

    if(screen != nil)
        [window setFrame:[screen visibleFrame] display:YES animate:NO];
}

-(void)_titlebarDoubleClicked {
    NSWindow *window = [self window];

    if([self _shouldFillOnDoubleClick] && ([window styleMask] & NSResizableWindowMask))
        [self _zoomFill:self];
    else if([self _shouldZoomOnDoubleClick] && ([window styleMask] & NSResizableWindowMask))
        [window performZoom:self];
    else if([self _shouldMiniaturizeOnDoubleClick] && ([window styleMask] & NSMiniaturizableWindowMask))
        [window performMiniaturize:self];
}

@end
