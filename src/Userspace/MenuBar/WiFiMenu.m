/*
 * WiFiMenu.m - the demo's Wi-Fi menu: the switch, the joined network, known and other
 * networks, Other Network and Wi-Fi Settings, drawn over PDWiFi.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "WiFiMenu.h"
#import "PDWiFiUI.h"
#import "MenuBarView.h"

#define MenuW 292
#define Inset 5
#define HeadH 32
#define NetH 34
#define TitleH 24
#define ItemH 24
#define SepH 11

static NSColor *c(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static BOOL dark(void)
{
    CFPropertyListRef style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);

    if (style == NULL)
        return NO;
    CFRelease(style);
    return YES;
}

static NSDictionary *t(NSString *face, CGFloat size, NSColor *colour)
{
    NSFont *f = [NSFont fontWithName:face size:size] ?: [NSFont systemFontOfSize:size];
    return @{ NSFontAttributeName: f, NSForegroundColorAttributeName: colour };
}

// one line of the menu: what it is, how tall, and what a click does
@interface WiFiItem : NSObject {
@public
    NSString *kind;
    NSString *title;
    PDWiFiNetwork *net;
    CGFloat y, h;
}
@end

@implementation WiFiItem
- (void)dealloc
{
    [kind release];
    [title release];
    [net release];
    [super dealloc];
}
@end

@interface WiFiMenuView : NSView {
@public
    NSMutableArray *_items;
    NSInteger _hover;
    WiFiMenu *_owner;
}
- (CGFloat)rebuild;
@end

@interface WiFiMenu () {
    NSPanel *_panel;
    WiFiMenuView *_view;
    NSTimer *_spin, *_rescan;
    CGFloat _x;
}
- (void)close;
- (void)relayout;
@end

@implementation WiFiMenuView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)add:(NSString *)kind title:(NSString *)title net:(PDWiFiNetwork *)net h:(CGFloat)h at:(CGFloat *)y
{
    WiFiItem *i = [[[WiFiItem alloc] init] autorelease];

    i->kind = [kind retain];
    i->title = [title retain];
    i->net = [net retain];
    i->y = *y;
    i->h = h;
    [_items addObject:i];
    *y += h;
}

// the demo's renderWifiMenu: the switch, the joined network, then known and other networks in range
- (CGFloat)rebuild
{
    PDWiFi *w = [PDWiFi shared];
    CGFloat y = Inset;
    NSMutableArray *known = [NSMutableArray array], *other = [NSMutableArray array];

    if (_items == nil)
        _items = [[NSMutableArray alloc] init];
    [_items removeAllObjects];
    [self add:@"head" title:@"Wi-Fi" net:nil h:HeadH at:&y];
    if (!w.hardware) {
        [self add:@"note" title:NSLocalizedString(@"No Wi-Fi hardware", @"") net:nil h:ItemH at:&y];
    } else if (w.powered) {
        for (PDWiFiNetwork *n in w.inRange) {
            if (w.current && [w.current.ssid isEqualToString:n.ssid])
                continue;
            [(n.known ? known : other) addObject:n];
        }
        if (w.current)
            [self add:@"net" title:w.current.ssid net:w.current h:NetH at:&y];
        if ([known count]) {
            [self add:@"title" title:NSLocalizedString(@"Known Networks", @"") net:nil h:TitleH at:&y];
            for (PDWiFiNetwork *n in known)
                [self add:@"net" title:n.ssid net:n h:NetH at:&y];
        }
        if ([other count]) {
            [self add:@"title" title:NSLocalizedString(@"Other Networks", @"") net:nil h:TitleH at:&y];
            for (PDWiFiNetwork *n in other)
                [self add:@"net" title:n.ssid net:n h:NetH at:&y];
        }
        if (![known count] && ![other count] && !w.current)
            [self add:@"note" title:w.scanning ? NSLocalizedString(@"Looking for networks…", @"") : NSLocalizedString(@"No networks in range", @"") net:nil h:ItemH at:&y];
        [self add:@"sep" title:nil net:nil h:SepH at:&y];
        [self add:@"other" title:NSLocalizedString(@"Other Network…", @"") net:nil h:ItemH at:&y];
    }
    [self add:@"sep" title:nil net:nil h:SepH at:&y];
    [self add:@"settings" title:NSLocalizedString(@"Wi-Fi Settings…", @"") net:nil h:ItemH at:&y];
    return y + Inset;
}

- (BOOL)clickable:(WiFiItem *)i
{
    return [i->kind isEqualToString:@"net"] || [i->kind isEqualToString:@"other"] || [i->kind isEqualToString:@"settings"];
}

- (NSRect)switchRect:(WiFiItem *)i
{
    return NSMakeRect(NSWidth([self bounds]) - Inset - 10 - 32, i->y + (i->h - 19) / 2, 32, 19);
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    PDWiFi *w = [PDWiFi shared];
    NSColor *ink = dark() ? c(0xee, 0xee, 0xee, 1) : c(0x1b, 0x1b, 0x1b, 1);
    NSColor *muted = c(0x8a, 0x8a, 0x8a, 1), *accent = PDSystemAccent();
    NSBezierPath *frame = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.5, 0.5) xRadius:8 yRadius:8];

    [[NSColor clearColor] set];
    NSRectFillUsingOperation(b, NSCompositeCopy);
    [(dark() ? c(44, 44, 46, 0.96) : c(250, 247, 245, 0.97)) setFill];
    [frame fill];
    [(dark() ? c(255, 255, 255, 0.12) : c(0, 0, 0, 0.12)) setStroke];
    [frame stroke];

    for (NSUInteger k = 0; k < [_items count]; k++) {
        WiFiItem *i = _items[k];
        NSRect r = NSMakeRect(Inset, i->y, NSWidth(b) - 2 * Inset, i->h);
        BOOL hot = (NSInteger)k == _hover && [self clickable:i];
        NSColor *fg = hot ? [NSColor whiteColor] : ink;

        if (hot) {
            [accent setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:5 yRadius:5] fill];
        }
        if ([i->kind isEqualToString:@"head"]) {
            NSRect sw = [self switchRect:i];
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMidY(r) - 8) withAttributes:t(@"Inter-SemiBold", 13, ink)];
            if (w.hardware) {
                [(w.powered ? accent : (dark() ? c(0x57, 0x51, 0x53, 1) : c(0xd8, 0xd3, 0xce, 1))) setFill];
                [[NSBezierPath bezierPathWithRoundedRect:sw xRadius:9.5 yRadius:9.5] fill];
                [[NSColor whiteColor] setFill];
                [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(w.powered ? NSMaxX(sw) - 17 : NSMinX(sw) + 2, NSMinY(sw) + 2, 15, 15)] fill];
            }
        } else if ([i->kind isEqualToString:@"title"]) {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMinY(r) + 7) withAttributes:t(@"Inter-SemiBold", 11, muted)];
        } else if ([i->kind isEqualToString:@"note"]) {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMidY(r) - 8) withAttributes:t(@"Inter-Regular", 13, muted)];
        } else if ([i->kind isEqualToString:@"sep"]) {
            [(dark() ? c(255, 255, 255, 0.14) : c(0, 0, 0, 0.12)) setFill];
            // a path fill blends, NSRectFill would punch the panel through to the desktop
            [[NSBezierPath bezierPathWithRect:NSMakeRect(NSMinX(r) + 3, NSMidY(r), NSWidth(r) - 6, 1)] fill];
        } else if ([i->kind isEqualToString:@"net"]) {
            BOOL on = w.current && [w.current.ssid isEqualToString:i->net.ssid];
            NSRect circle = NSMakeRect(NSMinX(r) + 3, NSMidY(r) - 13, 26, 26);
            NSColor *bg = on ? accent : hot ? c(255, 255, 255, 0.25) : (dark() ? c(255, 255, 255, 0.1) : c(0, 0, 0, 0.08));
            NSDictionary *na = t(@"Inter-Regular", 13, fg);
            NSString *name = i->title;

            [bg setFill];
            [[NSBezierPath bezierPathWithOvalInRect:circle] fill];
            PDDrawWiFiGlyph(NSInsetRect(circle, 6, 6), [i->net bars], on || hot ? [NSColor whiteColor] : ink, YES);
            while ([name length] > 1 && [name sizeWithAttributes:na].width > NSWidth(r) - 70)
                name = [[name substringToIndex:[name length] - 2] stringByAppendingString:@"…"];
            [name drawAtPoint:NSMakePoint(NSMinX(r) + 39, NSMidY(r) - 8) withAttributes:na];
            if ([w.busy isEqualToString:i->net.ssid])
                PDDrawWiFiSpinner(NSMakeRect(NSMaxX(r) - 20, NSMidY(r) - 6, 12, 12), hot ? [NSColor whiteColor] : accent);
            else if (i->net.security)
                PDDrawWiFiLock(NSMakeRect(NSMaxX(r) - 18, NSMidY(r) - 6, 10, 12), [fg colorWithAlphaComponent:hot ? 0.9 : 0.5], YES);
        } else {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMidY(r) - 8) withAttributes:t(@"Inter-Regular", 13, fg)];
        }
    }
}

- (NSInteger)itemAt:(NSPoint)p
{
    for (NSUInteger k = 0; k < [_items count]; k++) {
        WiFiItem *i = _items[k];
        if (p.y >= i->y && p.y < i->y + i->h)
            return (NSInteger)k;
    }
    return -1;
}

- (void)mouseMoved:(NSEvent *)e
{
    NSInteger h = [self itemAt:[self convertPoint:[e locationInWindow] fromView:nil]];

    if (h != _hover) {
        _hover = h;
        [self setNeedsDisplay:YES];
    }
}

- (void)mouseDown:(NSEvent *)e
{
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    NSInteger k = [self itemAt:p];
    PDWiFi *w = [PDWiFi shared];
    WiFiItem *i = k >= 0 ? _items[k] : nil;

    if (i == nil)
        return;
    if ([i->kind isEqualToString:@"head"] && w.hardware && NSPointInRect(p, NSInsetRect([self switchRect:i], -6, -6))) {
        [w setPower:!w.powered];
    } else if ([i->kind isEqualToString:@"net"]) {
        // a secured network nobody has joined asks for its password, the sheet replaces the menu
        if (i->net.security && !i->net.known && !(w.current && [w.current.ssid isEqualToString:i->net.ssid]))
            [_owner close];
        [w pick:i->net accent:PDSystemAccent()];
    } else if ([i->kind isEqualToString:@"other"]) {
        [_owner close];
        [w joinOther:PDSystemAccent()];
    } else if ([i->kind isEqualToString:@"settings"]) {
        [_owner close];
        // Settings opens on the pane named here, as the menu bar's other shortcuts do
        CFPreferencesSetAppValue(CFSTR("OpenPane"), CFSTR("network"), CFSTR("org.puredarwin.Settings"));
        CFPreferencesAppSynchronize(CFSTR("org.puredarwin.Settings"));
        [[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/Settings.app"];
    }
}

- (void)keyDown:(NSEvent *)e
{
    if ([[e characters] isEqualToString:@"\e"])
        [_owner close];
}

- (void)dealloc
{
    [_items release];
    [super dealloc];
}

@end

@implementation WiFiMenu

+ (WiFiMenu *)shared
{
    static WiFiMenu *shared;

    if (shared == nil)
        shared = [[WiFiMenu alloc] init];
    return shared;
}

- (BOOL)isOpen
{
    return _panel != nil && [_panel isVisible];
}

- (void)relayout
{
    NSRect screen = [[NSScreen mainScreen] frame];
    CGFloat h;

    if (![self isOpen])
        return;
    h = [_view rebuild];
    [_panel setFrame:NSMakeRect(MIN(_x, NSMaxX(screen) - MenuW - 6), NSMaxY(screen) - MenuBarHeight - 2 - h, MenuW, h) display:NO];
    [_view setFrame:NSMakeRect(0, 0, MenuW, h)];
    [_view setNeedsDisplay:YES];
}

- (void)changed:(NSNotification *)note
{
    [self relayout];
}

// the spinner turns while a network is being joined
- (void)spin:(NSTimer *)timer
{
    if ([PDWiFi shared].busy)
        [_view setNeedsDisplay:YES];
}

- (void)rescan:(NSTimer *)timer
{
    if ([PDWiFi shared].powered)
        [[PDWiFi shared] scan];
}

- (void)resigned:(NSNotification *)note
{
    [self close];
}

- (void)toggleAtX:(CGFloat)x
{
    if ([self isOpen]) {
        [self close];
        return;
    }
    if (_panel == nil) {
        _panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, MenuW, 100) styleMask:NSBorderlessWindowMask
                                              backing:NSBackingStoreBuffered defer:NO];
        [_panel setLevel:NSModalPanelWindowLevel];
        [_panel setOpaque:NO];
        [_panel setBackgroundColor:[NSColor clearColor]];
        [_panel setAcceptsMouseMovedEvents:YES];
        _view = [[WiFiMenuView alloc] initWithFrame:NSMakeRect(0, 0, MenuW, 100)];
        _view->_owner = self;
        [_panel setContentView:_view];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(changed:) name:PDWiFiChangedNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(resigned:) name:NSWindowDidResignKeyNotification
                                                   object:_panel];
    }
    _x = x;
    _view->_hover = -1;
    [_panel orderFront:nil];
    [self relayout];
    [_panel makeKeyAndOrderFront:nil];
    [_panel makeFirstResponder:_view];
    [[PDWiFi shared] refresh];
    [[PDWiFi shared] scan];
    _spin = [NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(spin:) userInfo:nil repeats:YES];
    _rescan = [NSTimer scheduledTimerWithTimeInterval:15 target:self selector:@selector(rescan:) userInfo:nil repeats:YES];
}

- (void)close
{
    [_spin invalidate];
    [_rescan invalidate];
    _spin = _rescan = nil;
    [_panel orderOut:nil];
    // the bar drops the glyph's highlight
    [[NSNotificationCenter defaultCenter] postNotificationName:PDWiFiChangedNotification object:nil];
}

@end
