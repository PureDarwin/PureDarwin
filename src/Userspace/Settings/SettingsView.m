/*
 * SettingsView.m - the Settings window: home tiles, panes, breadcrumbs and search.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "SettingsView.h"
#import "SettingsData.h"
#import "PDWiFiUI.h"

// sizes from the demo's settings window
#define TB_H 38
#define PAD 22
#define TILE_MIN 150
#define TILE_GAP 10
#define TILE_H 60
#define ROW_MIN 38
#define CARD_R 9
#define BANNER_H 116
#define LINE_SCROLL 24

typedef void (^HitBlock)(NSPoint p, BOOL dragging);
typedef void (^DrawBlock)(NSRect r);

// a colour that never changes, for previews that show one appearance whatever the current one is
static NSColor *fixed(int r, int g, int b)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1];
}

// the light palette and what each colour turns into in the dark, after the demo's dark rules
static const struct { unsigned light, dark; } palette[] = {
    { 0x1b1b1b, 0xe6e6e6 }, { 0x4a4a4a, 0xc8c8c8 }, { 0x6e6e6e, 0xa0a0a0 }, { 0x8a8a8a, 0x969696 },
    { 0xf4f1ee, 0x242224 }, { 0xf0ebe6, 0x3c383a }, { 0xf1ece7, 0x3a3537 }, { 0xd8d3ce, 0x4a4547 },
    { 0xdcd6d0, 0x1c1c1c }, { 0xc4c4c4, 0x555555 }, { 0x555555, 0xc4c4c4 }, { 0xfbfbfb, 0x3a3a3c },
    { 0xc9c2bb, 0x5a5456 }, { 0x9a8b7e, 0xa8998c }, { 0x8a7d72, 0x9a8d82 }, { 0x6b5d52, 0xd8ccc2 },
    { 0xb9aea4, 0x7a6f66 },
};

static BOOL darkNow(void)
{
    return [[NSAppearance currentDrawingAppearance] _isDark];
}

// palette colours follow the appearance, the rest are fixed
static NSColor *rgb(int r, int g, int b)
{
    unsigned light = (unsigned)(r << 16 | g << 8 | b);

    for (unsigned i = 0; i < sizeof(palette) / sizeof(palette[0]); i++) {
        if (palette[i].light != light)
            continue;
        unsigned dark = palette[i].dark;
        return [NSColor colorWithName:nil dynamicProvider:^NSColor *(NSAppearance *a) {
            unsigned v = [a _isDark] ? dark : light;
            return fixed(v >> 16, (v >> 8) & 0xff, v & 0xff);
        }];
    }
    return fixed(r, g, b);
}

// cards and controls sitting on the window background
static NSColor *surface(void)
{
    return darkNow() ? fixed(0x30, 0x2d, 0x2f) : [NSColor whiteColor];
}

static NSColor *rgba(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static NSFont *font(NSString *name, CGFloat size)
{
    NSFont *f = [NSFont fontWithName:name size:size];

    return f != nil ? f : [NSFont systemFontOfSize:size];
}

// per-category draw time, logged with PD_SETTINGS_TRACE
static double tBanner, tGlyph, tText, tCard;
#define TSTART NSDate *_t0 = [NSDate date]
#define TEND(acc) (acc) += -[_t0 timeIntervalSinceNow] * 1000

// the same few faces, sizes and colours every draw, built once
static NSDictionary *attrs(NSString *face, CGFloat size, NSColor *color)
{
    static NSMutableDictionary *cache;
    NSString *key = [NSString stringWithFormat:@"%@|%g|%@", face, size, color];
    NSDictionary *a = cache[key];

    if (a != nil)
        return a;
    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    a = @{ NSFontAttributeName: font(face, size), NSForegroundColorAttributeName: color };
    cache[key] = a;
    return a;
}

static NSImage *image(NSString *name)
{
    static NSMutableDictionary *cache;
    NSImage *img;

    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    if ((img = cache[name]) == nil) {
        NSString *path = [name isAbsolutePath] ? name : [[NSBundle mainBundle] pathForResource:name ofType:@"png"];

        img = path != nil ? [[[NSImage alloc] initWithContentsOfFile:path] autorelease] : nil;
        if (img == nil)
            return nil;
        cache[name] = img;
    }
    return img;
}

// draws an image to fill a rect, cropped to keep its aspect
static void drawFilled(NSImage *img, NSRect r)
{
    NSSize s = [img size];
    CGFloat scale;
    NSRect from;

    if (img == nil || s.width <= 0 || s.height <= 0)
        return;
    scale = MAX(NSWidth(r) / s.width, NSHeight(r) / s.height);
    from.size = NSMakeSize(NSWidth(r) / scale, NSHeight(r) / scale);
    from.origin = NSMakePoint((s.width - from.size.width) / 2, (s.height - from.size.height) / 2);
    [img drawInRect:r fromRect:from operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
}

// draws only the part of a screen-filling image that falls inside clip, so a small banner stays cheap
// the wallpaper scaled to the screen once, the banner then crops it 1:1 instead of resampling the full JPEG every draw
// the wallpaper scaled to the screen, built on a background queue so the window never waits for the JPEG
static NSImage *bannerPicture(NSString *path, NSSize screen, NSView *view)
{
    static NSImage *ready;
    static NSString *readyKey, *buildingKey;
    NSString *key = [NSString stringWithFormat:@"%@|%gx%g", path, screen.width, screen.height];

    if (ready != nil && [readyKey isEqualToString:key])
        return ready;
    if ([buildingKey isEqualToString:key] || screen.width <= 0 || screen.height <= 0)
        return nil;
    [buildingKey release];
    buildingKey = [key copy];
    [view retain];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSImage *full = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        NSSize s = [full size];
        NSImage *built = nil;

        if (full != nil && s.width > 0 && s.height > 0) {
            NSBitmapImageRep *rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:screen.width pixelsHigh:screen.height
                                                                          bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
                                                                         colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:32] autorelease];
            CGFloat scale = MAX(screen.width / s.width, screen.height / s.height);
            NSRect to = NSMakeRect((screen.width - s.width * scale) / 2, (screen.height - s.height * scale) / 2, s.width * scale, s.height * scale);

            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:rep]];
            [full drawInRect:to fromRect:NSZeroRect operation:NSCompositeCopy fraction:1];
            [NSGraphicsContext restoreGraphicsState];
            built = [[NSImage alloc] initWithSize:screen];
            [built addRepresentation:rep];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            if (built != nil && [buildingKey isEqualToString:key]) {
                [ready release];
                ready = built;
                [readyKey release];
                readyKey = [key copy];
                [view setNeedsDisplay:YES];
            } else {
                [built release];
            }
            [view release];
        });
        [pool release];
    });
    return nil;
}

static void drawPinned(NSImage *img, NSRect screen, NSRect clip)
{
    NSSize s = [img size];
    CGFloat scale;
    NSRect visible = NSIntersectionRect(screen, clip), from;
    CGFloat ox, oy;

    if (img == nil || s.width <= 0 || s.height <= 0 || NSIsEmptyRect(visible))
        return;
    scale = MAX(NSWidth(screen) / s.width, NSHeight(screen) / s.height);
    ox = (s.width * scale - NSWidth(screen)) / 2;
    oy = (s.height * scale - NSHeight(screen)) / 2;
    // the view is flipped and the image is not, so the source row counts up from the bottom
    from = NSMakeRect((NSMinX(visible) - NSMinX(screen) + ox) / scale,
                      s.height - (NSMaxY(visible) - NSMinY(screen) + oy) / scale,
                      NSWidth(visible) / scale, NSHeight(visible) / scale);
    [img drawInRect:visible fromRect:from operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
}

// boot-args split on whitespace, empty pieces dropped
static NSMutableArray *bootWords(void)
{
    NSMutableArray *words = [NSMutableArray array];

    for (NSString *w in [PDBootArgs() componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]) {
        if ([w length] > 0)
            [words addObject:w];
    }
    return words;
}

// each try is a full text layout, so the answer is kept for the next draw
static NSString *fitted(NSString *text, NSDictionary *a, CGFloat width)
{
    static NSMutableDictionary *cache;
    NSFont *f = a[NSFontAttributeName];
    NSString *key = [NSString stringWithFormat:@"%@|%@|%g|%g", text, [f fontName], [f pointSize], width];
    NSString *result = cache[key];
    NSUInteger n = [text length];

    if (result != nil)
        return result;
    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    result = @"…";
    if ([text sizeWithAttributes:a].width <= width)
        result = text;
    while (result != text && n > 1) {
        NSString *cut = [[text substringToIndex:--n] stringByAppendingString:@"…"];

        if ([cut sizeWithAttributes:a].width <= width) {
            result = cut;
            break;
        }
    }
    if ([cache count] > 2000)
        [cache removeAllObjects];
    cache[key] = result;
    return result;
}

// macOS's AppleAccentColor numbers, in the demo's swatch order
static const struct { int value; int r, g, b; } accents[] = {
    { 4, 0x2f, 0x6f, 0xdb }, { 5, 0x9b, 0x4f, 0xd1 }, { 6, 0xe0, 0x45, 0x7b }, { 0, 0xe5, 0x48, 0x3f },
    { 1, 0xf0, 0x8a, 0x24 }, { 2, 0xf2, 0xc2, 0x30 }, { 3, 0x4c, 0xae, 0x4f }, { -1, 0x8c, 0x8c, 0x8c },
};

static int accentValue(void)
{
    id v = PDGlobalPref(@"AppleAccentColor");

    return v != nil ? [v intValue] : 4;
}

static NSColor *accent(void)
{
    int v = accentValue();

    for (unsigned i = 0; i < sizeof(accents) / sizeof(accents[0]); i++) {
        if (accents[i].value == v)
            return rgb(accents[i].r, accents[i].g, accents[i].b);
    }
    return rgb(0x2f, 0x6f, 0xdb);
}

// id, name, subtitle, icon, glyph colour (nil draws the icon itself)
static NSArray *sections(void)
{
    return @[
        @[ NSLocalizedString(@"You", @""), @[ @[ @"appearance", NSLocalizedString(@"Appearance", @""), NSLocalizedString(@"Theme, wallpaper and dock", @""), @"pref-display" ],
                      @[ @"users", NSLocalizedString(@"Account", @""), NSLocalizedString(@"Name and picture", @""), @"pref-user" ] ] ],
        @[ NSLocalizedString(@"System", @""), @[ @[ @"boot", NSLocalizedString(@"Startup & Boot", @""), NSLocalizedString(@"Boot args and disk", @""), @"settings" ],
                         @[ @"services", NSLocalizedString(@"Services", @""), NSLocalizedString(@"launchd daemons", @""), @"terminal" ],
                         @[ @"kexts", NSLocalizedString(@"Kernel Extensions", @""), NSLocalizedString(@"Kernel collection", @""), @"folder" ],
                         @[ @"session", NSLocalizedString(@"Session", @""), NSLocalizedString(@"Compositor and windows", @""), @"pref-software" ] ] ],
        @[ NSLocalizedString(@"Hardware", @""), @[ @[ @"displays", NSLocalizedString(@"Displays", @""), NSLocalizedString(@"Resolution", @""), @"pref-display" ],
                           @[ @"network", NSLocalizedString(@"Network", @""), NSLocalizedString(@"Wi-Fi and Ethernet", @""), @"wifi", @"3d8fe6" ],
                           @[ @"sound", NSLocalizedString(@"Sound", @""), NSLocalizedString(@"Output devices", @""), @"volume", @"e2777a" ],
                           @[ @"power", NSLocalizedString(@"Power", @""), NSLocalizedString(@"Battery and sleep", @""), @"pref-power" ],
                           @[ @"input", NSLocalizedString(@"Mouse & Keyboard", @""), NSLocalizedString(@"Pointer and keys", @""), @"pref-mouse" ] ] ],
        @[ NSLocalizedString(@"Region", @""), @[ @[ @"clock", NSLocalizedString(@"Date & Time", @""), NSLocalizedString(@"Clock and time zone", @""), @"pref-locale" ],
                         @[ @"about", NSLocalizedString(@"About", @""), NSLocalizedString(@"Darwin and xnu", @""), @"settings" ] ] ],
    ];
}

static NSArray *paneInfo(NSString *pane)
{
    for (NSArray *s in sections()) {
        for (NSArray *p in s[1]) {
            if ([p[0] isEqualToString:pane])
                return p;
        }
    }
    return nil;
}

@interface SettingsHit : NSObject {
@public
    NSRect rect;
    HitBlock block;
}
@end

@implementation SettingsHit
- (void)dealloc
{
    [block release];
    [super dealloc];
}
@end

@implementation SettingsView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _pane = @"home";
    _history = [[NSMutableArray alloc] init];
    _search = [[NSMutableString alloc] init];
    _hits = [[NSMutableArray alloc] init];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(screenChanged:)
                                                 name:NSApplicationDidChangeScreenParametersNotification object:nil];
    // the banner shows the wallpaper behind the window, so it follows the window around
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowMoved:)
                                                 name:NSWindowDidMoveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(wifiChanged:)
                                                 name:PDWiFiChangedNotification object:nil];
    return self;
}

- (void)dealloc
{
    [_pane release];
    [_history release];
    [_search release];
    [_hits release];
    [_drag release];
    [_message release];
    [_daemons release];
    [_kexts release];
    [_bootArgs release];
    [_displayModes release];
    [_wifiScan invalidate];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

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

- (void)setMessage:(NSString *)message
{
    [_message release];
    _message = [message copy];
}

// panes that read the system load it when they open, so the window stays quick
- (void)loadPane:(NSString *)pane
{
    NSDate *started = [NSDate date];

    [self setMessage:nil];
    if ([pane isEqualToString:@"services"]) {
        [_daemons release];
        _daemons = [PDLaunchDaemons() retain];
    } else if ([pane isEqualToString:@"kexts"]) {
        [_kexts release];
        _kexts = [PDKernelExtensions() retain];
    } else if ([pane isEqualToString:@"boot"]) {
        [_bootArgs release];
        _bootArgs = [bootWords() retain];
    } else if ([pane isEqualToString:@"displays"]) {
        [_displayModes release];
        _displayModes = (NSArray *)CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
    }
    // the network pane keeps its list of networks fresh while it is open
    [_wifiScan invalidate];
    _wifiScan = nil;
    if ([pane isEqualToString:@"network"]) {
        [[PDWiFi shared] refresh];
        [[PDWiFi shared] scan];
        _wifiScan = [NSTimer scheduledTimerWithTimeInterval:20 target:self selector:@selector(wifiRescan:) userInfo:nil repeats:YES];
    }
    if (getenv("PD_SETTINGS_TRACE") != NULL)
        NSLog(@"Settings: load %@ %.1f ms", pane, -[started timeIntervalSinceNow] * 1000);
}

- (void)wifiRescan:(NSTimer *)t
{
    if ([[PDWiFi shared] powered])
        [[PDWiFi shared] scan];
}

- (void)wifiChanged:(NSNotification *)note
{
    if ([_pane isEqualToString:@"network"])
        [self setNeedsDisplay:YES];
}

- (void)windowMoved:(NSNotification *)note
{
    if ([note object] != [self window] || ![_pane isEqualToString:@"home"] || [_search length] > 0)
        return;
    [self setNeedsDisplayInRect:NSMakeRect(0, TB_H + 1 - _scroll, NSWidth([self bounds]), BANNER_H)];
}

// the switch lands later, when the compositor has resized the screen
- (void)screenChanged:(NSNotification *)note
{
    if ([_pane isEqualToString:@"displays"])
        [self loadPane:_pane];
    [self setNeedsDisplay:YES];
}

- (void)showPane:(NSString *)pane
{
    if (paneInfo(pane) == nil)
        pane = @"home";
    if (![pane isEqualToString:_pane])
        [_history addObject:_pane];
    [_pane release];
    _pane = [pane copy];
    _scroll = 0;
    [self loadPane:pane];
    [self setNeedsDisplay:YES];
}

- (IBAction)goBack:(id)sender
{
    NSString *pane;

    if ([_history count] == 0)
        return;
    pane = [[[_history lastObject] retain] autorelease];
    [_history removeLastObject];
    [_pane release];
    _pane = [pane copy];
    _scroll = 0;
    [self loadPane:pane];
    [self setNeedsDisplay:YES];
}

- (IBAction)goHome:(id)sender
{
    [self showPane:@"home"];
}

- (IBAction)find:(id)sender
{
    _searchActive = YES;
    [self setNeedsDisplay:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(goBack:))
        return [_history count] > 0;
    return YES;
}

- (void)hit:(NSRect)r block:(HitBlock)block
{
    SettingsHit *h = [[[SettingsHit alloc] init] autorelease];

    h->rect = r;
    h->block = [block copy];
    [_hits addObject:h];
}

// a row in a card: its height and how it draws itself into the rect it gets
- (NSDictionary *)row:(CGFloat)height draw:(DrawBlock)draw
{
    return @{ @"h": @(height), @"draw": [[draw copy] autorelease] };
}

- (void)drawLabel:(NSString *)label sub:(NSString *)sub in:(NSRect)r code:(BOOL)code
{
    NSDictionary *main = attrs(@"Inter-Regular", 13, rgb(0x1b, 0x1b, 0x1b));
    NSDictionary *small = code ? attrs(@"SourceCodePro-Regular", 11, rgb(0x8a, 0x7d, 0x72)) : attrs(@"Inter-Regular", 11, rgb(0x8a, 0x8a, 0x8a));
    CGFloat lh = 16, sh = sub != nil ? 14 : 0;
    CGFloat y = NSMinY(r) + floor((NSHeight(r) - lh - sh) / 2);

    [fitted(label, main, NSWidth(r)) drawAtPoint:NSMakePoint(NSMinX(r), y) withAttributes:main];
    if (sub != nil)
        [fitted(sub, small, NSWidth(r)) drawAtPoint:NSMakePoint(NSMinX(r), y + lh) withAttributes:small];
}

- (NSDictionary *)valueRow:(NSString *)label value:(NSString *)value
{
    return [self row:ROW_MIN draw:^(NSRect r) {
        NSDictionary *a = attrs(@"Inter-Regular", 13, rgb(0x6e, 0x6e, 0x6e));
        NSString *v = fitted(value ?: @"", a, NSWidth(r) * 0.62);
        NSSize s = [v sizeWithAttributes:a];

        [self drawLabel:label sub:nil in:NSMakeRect(NSMinX(r) + 12, NSMinY(r), NSWidth(r) - 24 - s.width - 12, NSHeight(r)) code:NO];
        [v drawAtPoint:NSMakePoint(NSMaxX(r) - 12 - s.width, NSMinY(r) + floor((NSHeight(r) - s.height) / 2)) withAttributes:a];
    }];
}

- (void)drawSwitch:(NSRect)r on:(BOOL)on enabled:(BOOL)enabled
{
    NSRect knob = NSMakeRect(on ? NSMaxX(r) - 17 : NSMinX(r) + 2, NSMinY(r) + 2, 15, 15);

    [(on ? accent() : rgb(0xd8, 0xd3, 0xce)) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:r xRadius:9.5 yRadius:9.5] fill];
    [[NSColor colorWithCalibratedWhite:0 alpha:0.18] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSOffsetRect(knob, 0, 0.8)] fill];
    [[NSColor whiteColor] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:knob] fill];
    if (!enabled) {
        [[NSColor colorWithCalibratedWhite:1 alpha:0.5] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, -1, -1) xRadius:10 yRadius:10] fill];
    }
}

- (NSDictionary *)switchRow:(NSString *)label sub:(NSString *)sub code:(BOOL)code dot:(NSColor *)dot
                         on:(BOOL)on enabled:(BOOL)enabled toggle:(void (^)(BOOL))toggle
{
    return [self row:(sub != nil ? 46 : ROW_MIN) draw:^(NSRect r) {
        NSRect sw = NSMakeRect(NSMaxX(r) - 12 - 32, NSMidY(r) - 9.5, 32, 19);
        CGFloat x = NSMinX(r) + 12;

        if (dot != nil) {
            [dot setFill];
            [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, NSMinY(r) + floor((NSHeight(r) - (sub != nil ? 30 : 16)) / 2) + 4, 8, 8)] fill];
            x += 14;
        }
        [self drawLabel:label sub:sub in:NSMakeRect(x, NSMinY(r), NSMinX(sw) - 12 - x, NSHeight(r)) code:code];
        [self drawSwitch:sw on:on enabled:enabled];
        if (enabled) {
            [self hit:NSInsetRect(sw, -4, -6) block:^(NSPoint p, BOOL dragging) {
                if (!dragging)
                    toggle(!on);
            }];
        }
    }];
}

- (NSDictionary *)sliderRow:(NSString *)label value:(CGFloat)value change:(void (^)(CGFloat, BOOL))change
{
    return [self row:ROW_MIN draw:^(NSRect r) {
        NSRect track = NSMakeRect(NSMaxX(r) - 12 - 200, NSMidY(r) - 2, 200, 4);
        CGFloat kx = NSMinX(track) + value * NSWidth(track);

        [self drawLabel:label sub:nil in:NSMakeRect(NSMinX(r) + 12, NSMinY(r), NSMinX(track) - 24 - NSMinX(r), NSHeight(r)) code:NO];
        [rgb(0xd8, 0xd3, 0xce) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:track xRadius:2 yRadius:2] fill];
        [accent() setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSMinX(track), NSMinY(track), kx - NSMinX(track), 4) xRadius:2 yRadius:2] fill];
        [[NSColor colorWithCalibratedWhite:0 alpha:0.2] setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(kx - 8, NSMidY(r) - 7.5, 16, 16)] fill];
        [[NSColor whiteColor] setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(kx - 7.5, NSMidY(r) - 8, 15, 15)] fill];
        [self hit:NSMakeRect(NSMinX(track) - 8, NSMinY(r), NSWidth(track) + 16, NSHeight(r)) block:^(NSPoint p, BOOL dragging) {
            change(MIN(MAX((p.x - NSMinX(track)) / NSWidth(track), 0), 1), dragging);
        }];
    }];
}

- (void)popupPicked:(NSMenuItem *)item
{
    void (^pick)(void) = [item representedObject];

    pick();
}

// a pop-up button like macOS's, the menu opens over it with the chosen item under the pointer
- (NSDictionary *)popupRow:(NSString *)label items:(NSArray *)items selected:(NSUInteger)selected pick:(void (^)(NSUInteger))pick
{
    return [self row:ROW_MIN draw:^(NSRect r) {
        NSDictionary *a = attrs(@"Inter-Regular", 12, rgb(0x1b, 0x1b, 0x1b));
        NSString *title = selected < [items count] ? items[selected] : @"";
        CGFloat w = 0;

        for (NSString *i in items)
            w = MAX(w, [i sizeWithAttributes:a].width);

        NSRect b = NSMakeRect(NSMaxX(r) - 12 - w - 38, NSMidY(r) - 11, w + 38, 22);
        NSBezierPath *chevron = [NSBezierPath bezierPath];
        CGFloat cx = NSMaxX(b) - 14, cy = NSMidY(b);

        [self drawLabel:label sub:nil in:NSMakeRect(NSMinX(r) + 12, NSMinY(r), NSMinX(b) - 24 - NSMinX(r), NSHeight(r)) code:NO];
        [[NSColor colorWithCalibratedWhite:0 alpha:0.12] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSOffsetRect(b, 0, 0.5) xRadius:5 yRadius:5] fill];
        [(darkNow() ? fixed(0x57, 0x51, 0x53) : [NSColor whiteColor]) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:b xRadius:5 yRadius:5] fill];
        [title drawAtPoint:NSMakePoint(NSMinX(b) + 10, NSMidY(b) - 8) withAttributes:a];
        [chevron moveToPoint:NSMakePoint(cx - 3, cy - 2)];
        [chevron lineToPoint:NSMakePoint(cx, cy - 5)];
        [chevron lineToPoint:NSMakePoint(cx + 3, cy - 2)];
        [chevron moveToPoint:NSMakePoint(cx - 3, cy + 2)];
        [chevron lineToPoint:NSMakePoint(cx, cy + 5)];
        [chevron lineToPoint:NSMakePoint(cx + 3, cy + 2)];
        [chevron setLineWidth:1.3];
        [rgb(0x4a, 0x4a, 0x4a) setStroke];
        [chevron stroke];
        [self hit:b block:^(NSPoint p, BOOL dragging) {
            NSMenu *menu = [[[NSMenu alloc] initWithTitle:label] autorelease];
            NSMenuItem *chosen = nil;

            // opens on the press like a pop-up button, the menu then owns the drag and the release
            if (!dragging || [NSApp currentEvent] == nil || [[NSApp currentEvent] type] != NSLeftMouseDown)
                return;
            for (NSUInteger i = 0; i < [items count]; i++) {
                NSMenuItem *item = [menu addItemWithTitle:items[i] action:@selector(popupPicked:) keyEquivalent:@""];

                [item setTarget:self];
                [item setRepresentedObject:[[^{ pick(i); } copy] autorelease]];
                if (i == selected) {
                    [item setState:NSOnState];
                    chosen = item;
                }
            }
            [menu popUpMenuPositioningItem:chosen atLocation:NSMakePoint(NSMinX(b) + 4, NSMidY(b)) inView:self];
            [_drag release];
            _drag = nil;
        }];
    }];
}

- (NSDictionary *)buttonRow:(NSString *)title note:(NSString *)note action:(void (^)(void))action
{
    return [self row:ROW_MIN + 4 draw:^(NSRect r) {
        NSDictionary *a = attrs(@"Inter-Regular", 12, rgb(0x1b, 0x1b, 0x1b));
        NSSize s = [title sizeWithAttributes:a];
        NSRect b = NSMakeRect(NSMaxX(r) - 12 - s.width - 28, NSMidY(r) - 11, s.width + 28, 22);

        if (note != nil)
            [self drawLabel:note sub:nil in:NSMakeRect(NSMinX(r) + 12, NSMinY(r), NSMinX(b) - 24 - NSMinX(r), NSHeight(r)) code:NO];
        [rgb(0xc4, 0xc4, 0xc4) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:b xRadius:6 yRadius:6] fill];
        [rgb(0xfb, 0xfb, 0xfb) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 1, 1) xRadius:5 yRadius:5] fill];
        [title drawAtPoint:NSMakePoint(NSMinX(b) + 14, NSMinY(b) + floor((22 - s.height) / 2)) withAttributes:a];
        [self hit:b block:^(NSPoint p, BOOL dragging) {
            if (!dragging)
                action();
        }];
    }];
}

// the demo's card: white, rounded, a hairline ring, rows split by faint rules
- (CGFloat)card:(NSArray *)rows at:(CGFloat)y
{
    CGFloat x = PAD, w = NSWidth([self bounds]) - 2 * PAD, h = 0, ry;
    NSRect box;

    for (NSDictionary *row in rows)
        h += [row[@"h"] doubleValue];
    box = NSMakeRect(x, y, w, h);
    [(darkNow() ? [NSColor colorWithCalibratedWhite:1 alpha:0.06] : rgba(80, 50, 30, 0.1)) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(box, -1, -1) xRadius:CARD_R + 1 yRadius:CARD_R + 1] fill];
    [surface() setFill];
    [[NSBezierPath bezierPathWithRoundedRect:box xRadius:CARD_R yRadius:CARD_R] fill];
    ry = y;
    for (NSUInteger i = 0; i < [rows count]; i++) {
        CGFloat rh = [rows[i][@"h"] doubleValue];

        if (i > 0) {
            [rgb(0xf0, 0xeb, 0xe6) setFill];
            NSRectFill(NSMakeRect(x + 12, ry, w - 24, 1));
        }
        ((DrawBlock)rows[i][@"draw"])(NSMakeRect(x, ry, w, rh));
        ry += rh;
    }
    return y + h + 14;
}

- (CGFloat)title:(NSString *)text at:(CGFloat)y
{
    NSMutableAttributedString *s = [[[NSMutableAttributedString alloc] initWithString:[text uppercaseString]
                                                                           attributes:attrs(@"Inter-SemiBold", 11, rgb(0x9a, 0x8b, 0x7e))] autorelease];

    [s addAttribute:NSKernAttributeName value:@(0.66) range:NSMakeRange(0, [s length])];
    [s drawAtPoint:NSMakePoint(PAD + 2, y)];
    return y + 20;
}

- (CGFloat)lead:(NSString *)text at:(CGFloat)y
{
    [text drawAtPoint:NSMakePoint(PAD, y) withAttributes:attrs(@"Inter-Regular", 12, rgb(0x6e, 0x6e, 0x6e))];
    return y + 28;
}

// the wallpaper behind the banner is pinned to the screen, so it lines up with the desktop around the window
- (void)drawBanner:(NSRect)r
{
    NSString *name = PDPref(PDDockDomain, @"DesktopPicture") ?: @"Sunset";
    NSString *path = [name isAbsolutePath] ? name : [NSString stringWithFormat:@"/System/Library/Desktop Pictures/%@.jpg", name];
    NSRect screen = [[NSScreen mainScreen] frame], win = [[self window] frame];
    NSRect pinned = NSMakeRect(NSMinX(screen) - NSMinX(win), NSMaxY(win) - NSMaxY(screen), NSWidth(screen), NSHeight(screen));
    NSDictionary *white = attrs(@"Inter-Regular", 12, [NSColor colorWithCalibratedWhite:1 alpha:0.92]);
    NSString *host = [NSString stringWithFormat:@"%@ · %@", PDHostName(), NSUserName() ?: @"root"];
    NSSize hs = [host sizeWithAttributes:white];
    NSImage *logo = image(@"beak-white");

    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:r];
    [rgb(0xd9, 0x77, 0x5a) setFill];
    NSRectFill(r);
    // the picture is decoded and scaled off the main thread, until then the banner shows its tint alone
    NSImage *picture = bannerPicture(path, screen.size, self);

    if (picture != nil)
        drawPinned(picture, pinned, r);
    [[NSColor colorWithCalibratedWhite:0 alpha:0.16] setFill];
    [[NSBezierPath bezierPathWithRect:r] fill];
    [logo drawInRect:NSMakeRect(NSMaxX(r) - 170, NSMaxY(r) - 160, 200, 200) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:0.14 respectFlipped:YES hints:nil];
    [logo drawInRect:NSMakeRect(NSMinX(r) + 26, NSMidY(r) - 28, 56, 56) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    [[NSString stringWithFormat:NSLocalizedString(@"PureDarwin %@", @""), PDSystemVersion()] drawAtPoint:NSMakePoint(NSMinX(r) + 100, NSMinY(r) + 22)
                                                                  withAttributes:attrs(@"Inter-Bold", 22, [NSColor whiteColor])];
    [[NSString stringWithFormat:NSLocalizedString(@"%1$@ · %2$@ · %3$@ · build %4$@", @""), PDKernelVersion(), PDXNUTag(), PDProcessor(), PDBuildVersion()]
        drawAtPoint:NSMakePoint(NSMinX(r) + 100, NSMinY(r) + 52) withAttributes:white];
    [[NSColor colorWithCalibratedWhite:1 alpha:0.18] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSMinX(r) + 100, NSMinY(r) + 74, hs.width + 18, 22) xRadius:11 yRadius:11] fill];
    [host drawAtPoint:NSMakePoint(NSMinX(r) + 109, NSMinY(r) + 74 + floor((22 - hs.height) / 2)) withAttributes:white];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawGlyph:(NSArray *)tile in:(NSRect)r
{
    if ([tile count] > 4) {
        unsigned c = 0;

        sscanf([tile[4] UTF8String], "%x", &c);
        [rgb(c >> 16, c >> 8 & 0xff, c & 0xff) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:r xRadius:8 yRadius:8] fill];
        [image(tile[3]) drawInRect:NSInsetRect(r, 6.5, 6.5) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        return;
    }
    [image(tile[3]) drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
}

- (CGFloat)drawHomeAt:(CGFloat)y
{
    CGFloat w = NSWidth([self bounds]) - 2 * PAD;
    NSInteger cols = MAX(1, (NSInteger)floor((w + TILE_GAP) / (TILE_MIN + TILE_GAP)));
    CGFloat tw = floor((w - (cols - 1) * TILE_GAP) / cols);
    NSString *filter = [_search lowercaseString];
    BOOL any = NO;

    if ([filter length] == 0) {
        { TSTART; [self drawBanner:NSMakeRect(0, y, NSWidth([self bounds]), BANNER_H)]; TEND(tBanner); }
        y += BANNER_H;
        // a move only invalidates the banner, the tiles below are already on screen
        if (NSMaxY(_dirty) <= y)
            return _contentHeight + TB_H - _scroll - 10;
    }
    y += 16;
    for (NSArray *section in sections()) {
        NSMutableArray *tiles = [NSMutableArray array];
        NSInteger i;

        for (NSArray *t in section[1]) {
            NSString *text = [[NSString stringWithFormat:@"%@ %@", t[1], t[2]] lowercaseString];

            if ([filter length] == 0 || [text rangeOfString:filter].location != NSNotFound)
                [tiles addObject:t];
        }
        if ([tiles count] == 0)
            continue;
        any = YES;
        y = [self title:section[0] at:y];
        for (i = 0; i < (NSInteger)[tiles count]; i++) {
            NSArray *t = tiles[i];
            NSRect r = NSMakeRect(PAD + (i % cols) * (tw + TILE_GAP), y + (i / cols) * (TILE_H + TILE_GAP), tw, TILE_H);
            NSDictionary *bold = attrs(@"Inter-Bold", 12.5, rgb(0x1b, 0x1b, 0x1b));
            NSDictionary *small = attrs(@"Inter-Regular", 11, rgb(0x8a, 0x8a, 0x8a));
            CGFloat tx = NSMinX(r) + 12 + 30 + 10;

            { TSTART;
            [(darkNow() ? [NSColor colorWithCalibratedWhite:1 alpha:0.06] : rgba(80, 50, 30, 0.1)) setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSOffsetRect(NSInsetRect(r, -1, -1), 0, 0.5) xRadius:CARD_R + 1 yRadius:CARD_R + 1] fill];
            [surface() setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:CARD_R yRadius:CARD_R] fill];
            TEND(tCard); }
            { TSTART; [self drawGlyph:t in:NSMakeRect(NSMinX(r) + 12, NSMidY(r) - 15, 30, 30)]; TEND(tGlyph); }
            { TSTART;
            [fitted(t[1], bold, NSMaxX(r) - 10 - tx) drawAtPoint:NSMakePoint(tx, NSMidY(r) - 16) withAttributes:bold];
            [fitted(t[2], small, NSMaxX(r) - 10 - tx) drawAtPoint:NSMakePoint(tx, NSMidY(r) + 1) withAttributes:small];
            TEND(tText); }
            [self hit:r block:^(NSPoint p, BOOL dragging) {
                if (dragging)
                    return;
                [_search setString:@""];
                _searchActive = NO;
                [self showPane:t[0]];
            }];
        }
        y += ((([tiles count] + cols - 1) / cols)) * (TILE_H + TILE_GAP) + 10;
    }
    if (!any)
        [[NSString stringWithFormat:NSLocalizedString(@"No settings match “%@”.", @""), _search] drawAtPoint:NSMakePoint(PAD, y)
                                                                             withAttributes:attrs(@"Inter-Regular", 13, rgb(0x6e, 0x6e, 0x6e))];
    return y;
}

- (CGFloat)drawAppearanceAt:(CGFloat)y
{
    NSString *look = PDGlobalPref(@"AppleInterfaceStyle") ? @"dark" : [PDGlobalPref(@"AppleInterfaceStyleSwitchesAutomatically") boolValue] ? @"auto" : @"light";
    NSString *wall = PDPref(PDDockDomain, @"DesktopPicture") ?: @"Sunset";
    CGFloat size = PDPref(PDDockDomain, @"TileSize") ? [PDPref(PDDockDomain, @"TileSize") doubleValue] : 48;
    BOOL magnify = PDPref(PDDockDomain, @"Magnify") ? [PDPref(PDDockDomain, @"Magnify") boolValue] : YES;

    y = [self title:NSLocalizedString(@"Theme", @"") at:y];
    y = [self card:@[
        [self row:72 draw:^(NSRect r) {
            NSArray *looks = @[ @[ @"light", NSLocalizedString(@"Light", @"") ], @[ @"dark", NSLocalizedString(@"Dark", @"") ], @[ @"auto", NSLocalizedString(@"Auto", @"") ] ];
            CGFloat x = NSMaxX(r) - 12 - 3 * 64 - 2 * 16;

            [self drawLabel:NSLocalizedString(@"Mode", @"") sub:nil in:NSMakeRect(NSMinX(r) + 12, NSMinY(r), 200, NSHeight(r)) code:NO];
            for (NSArray *l in looks) {
                NSRect p = NSMakeRect(x, NSMinY(r) + 8, 64, 42);
                NSDictionary *a = attrs(@"Inter-Regular", 11, rgb(0x4a, 0x4a, 0x4a));
                NSSize s = [l[1] sizeWithAttributes:a];
                BOOL on = [look isEqualToString:l[0]];

                // a hairline outline, or the accent ring in its place when chosen
                [(on ? accent() : [NSColor colorWithCalibratedWhite:0 alpha:0.15]) setFill];
                CGFloat ring = on ? 3 : 1;
                [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(p, -ring, -ring) xRadius:6 + ring yRadius:6 + ring] fill];
                [NSGraphicsContext saveGraphicsState];
                [[NSBezierPath bezierPathWithRoundedRect:p xRadius:6 yRadius:6] addClip];
                if ([l[0] isEqualToString:@"auto"]) {
                    [fixed(0xf4, 0xf1, 0xee) setFill];
                    NSRectFill(NSMakeRect(NSMinX(p), NSMinY(p), 32, 42));
                    [rgb(0x2c, 0x2a, 0x2c) setFill];
                    NSRectFill(NSMakeRect(NSMinX(p) + 32, NSMinY(p), 32, 42));
                } else {
                    BOOL dark = [l[0] isEqualToString:@"dark"];

                    [(dark ? fixed(0x24, 0x22, 0x24) : [NSColor whiteColor]) setFill];
                    NSRectFill(p);
                    [(dark ? fixed(0x3a, 0x35, 0x37) : fixed(0xf4, 0xf1, 0xee)) setFill];
                    NSRectFill(NSMakeRect(NSMinX(p), NSMinY(p), 64, 13));
                }
                [NSGraphicsContext restoreGraphicsState];
                [l[1] drawAtPoint:NSMakePoint(NSMidX(p) - s.width / 2, NSMaxY(p) + 4) withAttributes:a];
                [self hit:NSInsetRect(p, -3, -3) block:^(NSPoint pt, BOOL dragging) {
                    if (dragging)
                        return;
                    PDSetGlobalPref(@"AppleInterfaceStyle", [l[0] isEqualToString:@"dark"] ? @"Dark" : nil);
                    PDSetGlobalPref(@"AppleInterfaceStyleSwitchesAutomatically", [l[0] isEqualToString:@"auto"] ? @YES : nil);
                    // running apps restyle on the same notification macOS sends
                    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"AppleInterfaceThemeChangedNotification" object:nil];
                    [self setNeedsDisplay:YES];
                }];
                x += 64 + 16;
            }
        }],
        [self row:ROW_MIN draw:^(NSRect r) {
            CGFloat x = NSMaxX(r) - 12 - 8 * 16 - 7 * 8;

            [self drawLabel:NSLocalizedString(@"Accent", @"") sub:nil in:NSMakeRect(NSMinX(r) + 12, NSMinY(r), 200, NSHeight(r)) code:NO];
            for (unsigned i = 0; i < sizeof(accents) / sizeof(accents[0]); i++) {
                NSRect dot = NSMakeRect(x, NSMidY(r) - 8, 16, 16);
                int value = accents[i].value;

                if (value == accentValue()) {
                    [rgb(accents[i].r, accents[i].g, accents[i].b) setFill];
                    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dot, -3, -3)] fill];
                    [[NSColor whiteColor] setFill];
                    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dot, -1.5, -1.5)] fill];
                }
                [rgb(accents[i].r, accents[i].g, accents[i].b) setFill];
                [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
                [self hit:NSInsetRect(dot, -3, -3) block:^(NSPoint p, BOOL dragging) {
                    if (dragging)
                        return;
                    PDSetGlobalPref(@"AppleAccentColor", value == 4 ? nil : @(value));
                    [self setNeedsDisplay:YES];
                }];
                x += 16 + 8;
            }
        }],
        [self switchRow:@"Rounded window corners" sub:nil code:NO dot:nil on:PDWindowStyleEnabled(@"PDWindowRoundedCorners") enabled:YES toggle:^(BOOL on) {
            PDSetGlobalPref(@"PDWindowRoundedCorners", on ? nil : @NO);
            PDPushWindowStyle();
            [self setNeedsDisplay:YES];
        }],
        [self switchRow:@"Window shadows" sub:nil code:NO dot:nil on:PDWindowStyleEnabled(@"PDWindowShadows") enabled:YES toggle:^(BOOL on) {
            PDSetGlobalPref(@"PDWindowShadows", on ? nil : @NO);
            PDPushWindowStyle();
            [self setNeedsDisplay:YES];
        }],
    ] at:y];

    y = [self title:NSLocalizedString(@"Wallpaper", @"") at:y];
    y = [self card:@[
        [self row:126 draw:^(NSRect r) {
            NSArray *walls = @[ NSLocalizedString(@"Sunset", @""), NSLocalizedString(@"Lagoon", @""), NSLocalizedString(@"Dusk", @""), NSLocalizedString(@"Graphite", @"") ];
            CGFloat gap = 10, w = floor((NSWidth(r) - 24 - 3 * gap) / 4);

            for (NSUInteger i = 0; i < [walls count]; i++) {
                NSRect t = NSMakeRect(NSMinX(r) + 12 + i * (w + gap), NSMinY(r) + 12, w, NSHeight(r) - 24);
                NSString *name = walls[i];

                if ([wall isEqualToString:name]) {
                    [accent() setFill];
                    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(t, -3, -3) xRadius:9 yRadius:9] fill];
                }
                [NSGraphicsContext saveGraphicsState];
                [[NSBezierPath bezierPathWithRoundedRect:t xRadius:7 yRadius:7] addClip];
                // the small copies Dock installs, decoding four full pictures would stall every redraw
                drawFilled(image([NSString stringWithFormat:@"/System/Library/Desktop Pictures/.thumbnails/%@.jpg", name]), t);
                [NSGraphicsContext restoreGraphicsState];
                [self hit:t block:^(NSPoint p, BOOL dragging) {
                    if (dragging)
                        return;
                    PDSetPref(PDDockDomain, @"DesktopPicture", name);
                    [self setNeedsDisplay:YES];
                }];
            }
        }],
    ] at:y];

    y = [self title:NSLocalizedString(@"Dock", @"") at:y];
    y = [self card:@[
        [self sliderRow:NSLocalizedString(@"Size", @"") value:(size - 32) / 40 change:^(CGFloat v, BOOL dragging) {
            PDSetPref(PDDockDomain, @"TileSize", @(round(32 + v * 40)));
            [self setNeedsDisplay:YES];
        }],
        [self switchRow:NSLocalizedString(@"Magnify on hover", @"") sub:nil code:NO dot:nil on:magnify enabled:YES toggle:^(BOOL on) {
            PDSetPref(PDDockDomain, @"Magnify", @(on));
            [self setNeedsDisplay:YES];
        }],
    ] at:y];
    if (![look isEqualToString:@"light"])
        y = [self lead:NSLocalizedString(@"Dark mode is saved, apps draw it once AppKit has a dark appearance.", @"") at:y];
    return y;
}

- (CGFloat)drawAccountAt:(CGFloat)y
{
    NSDictionary *a = PDAccount();

    return [self card:@[
        [self valueRow:NSLocalizedString(@"Full name", @"") value:a[@"name"]],
        [self valueRow:NSLocalizedString(@"Account name", @"") value:a[@"short"]],
        [self valueRow:NSLocalizedString(@"Account", @"") value:[a[@"uid"] intValue] == 0 ? NSLocalizedString(@"Administrator, uid 0", @"") : [NSString stringWithFormat:NSLocalizedString(@"Standard, uid %@", @""), a[@"uid"]]],
        [self valueRow:NSLocalizedString(@"Home", @"") value:a[@"home"]],
        [self valueRow:NSLocalizedString(@"Shell", @"") value:a[@"shell"]],
    ] at:y];
}

- (CGFloat)drawBootAt:(CGFloat)y
{
    NSArray *choices = @[ @"-v", @"debug=0x8", @"pdhaltlog=1", @"pdtrace=1", @"keepsyms=1", @"serial=3", @"-s" ];
    NSMutableArray *chips = [NSMutableArray arrayWithArray:choices];
    NSString *line = [_bootArgs componentsJoinedByString:@" "];
    BOOL changed = ![line isEqualToString:[bootWords() componentsJoinedByString:@" "]];

    // arguments already in use show as chips too, so they can be turned off
    for (NSString *a in _bootArgs) {
        if (![chips containsObject:a])
            [chips addObject:a];
    }
    y = [self lead:NSLocalizedString(@"Arguments passed to xnu at the next start, kept in boot-args.txt on the EFI partition.", @"") at:y];
    y = [self title:NSLocalizedString(@"Boot arguments", @"") at:y];
    y = [self card:@[
        [self row:0 draw:^(NSRect r) {}],
        [self row:(CGFloat)(28 * ceil([chips count] / 7.0) + 16) draw:^(NSRect r) {
            NSDictionary *mono = attrs(@"SourceCodePro-Regular", 11.5, rgb(0x6b, 0x5d, 0x52));
            NSDictionary *monoOn = attrs(@"SourceCodePro-Regular", 11.5, [NSColor whiteColor]);
            CGFloat x = NSMinX(r) + 12, cy = NSMinY(r) + 10;

            for (NSString *chip in chips) {
                BOOL on = [_bootArgs containsObject:chip];
                NSSize s = [chip sizeWithAttributes:mono];
                NSRect c = NSMakeRect(x, cy, s.width + 18, 22);

                if (NSMaxX(c) > NSMaxX(r) - 12) {
                    x = NSMinX(r) + 12;
                    cy += 28;
                    c = NSMakeRect(x, cy, s.width + 18, 22);
                }
                if (!on) {
                    [rgba(80, 50, 30, 0.1) setFill];
                    [[NSBezierPath bezierPathWithRoundedRect:c xRadius:11 yRadius:11] fill];
                }
                [(on ? accent() : rgb(0xf1, 0xec, 0xe7)) setFill];
                [[NSBezierPath bezierPathWithRoundedRect:(on ? c : NSInsetRect(c, 1, 1)) xRadius:11 yRadius:11] fill];
                [chip drawAtPoint:NSMakePoint(NSMinX(c) + 9, NSMinY(c) + floor((22 - s.height) / 2)) withAttributes:on ? monoOn : mono];
                [self hit:c block:^(NSPoint p, BOOL dragging) {
                    if (dragging)
                        return;
                    if ([_bootArgs containsObject:chip])
                        [_bootArgs removeObject:chip];
                    else
                        [_bootArgs addObject:chip];
                    [self setMessage:nil];
                    [self setNeedsDisplay:YES];
                }];
                x = NSMaxX(c) + 6;
            }
        }],
        [self row:40 draw:^(NSRect r) {
            NSRect box = NSMakeRect(NSMinX(r) + 12, NSMinY(r) + 4, NSWidth(r) - 24, 28);
            NSDictionary *mono = attrs(@"SourceCodePro-Regular", 11.5, rgb(0xd8, 0xd8, 0xd8));
            NSDictionary *monoBold = attrs(@"SourceCodePro-Bold", 11.5, rgb(0xd8, 0xd8, 0xd8));

            [rgb(0x1f, 0x20, 0x23) setFill];
            [[NSBezierPath bezierPathWithRoundedRect:box xRadius:6 yRadius:6] fill];
            [@"boot-args" drawAtPoint:NSMakePoint(NSMinX(box) + 9, NSMinY(box) + 7) withAttributes:monoBold];
            [fitted([NSString stringWithFormat:@"= %@", [line length] > 0 ? line : NSLocalizedString(@"(none)", @"")], mono, NSWidth(box) - 100)
                drawAtPoint:NSMakePoint(NSMinX(box) + 9 + [@"boot-args " sizeWithAttributes:monoBold].width, NSMinY(box) + 7) withAttributes:mono];
        }],
        [self buttonRow:NSLocalizedString(@"Save", @"") note:_message ?: (changed ? NSLocalizedString(@"Not saved yet.", @"") : NSLocalizedString(@"Matches the running system.", @"")) action:^{
            NSString *error = PDWriteBootArgs([_bootArgs componentsJoinedByString:@" "]);

            [self setMessage:error ?: NSLocalizedString(@"Saved, used at the next start.", @"")];
            [self setNeedsDisplay:YES];
        }],
    ] at:y];
    y = [self title:NSLocalizedString(@"Startup", @"") at:y];
    return [self card:@[
        [self valueRow:NSLocalizedString(@"Startup disk", @"") value:PDStartupDisk()],
        [self valueRow:NSLocalizedString(@"Firmware", @"") value:NSLocalizedString(@"UEFI, legacy BIOS supported", @"")],
        [self valueRow:@"Kernel collection" value:@"BootKernelExtensions.kc"],
    ] at:y];
}

- (CGFloat)drawServicesAt:(CGFloat)y
{
    NSMutableArray *rows = [NSMutableArray array];

    y = [self lead:NSLocalizedString(@"Daemons launchd starts at boot. Changes apply after a restart.", @"") at:y];
    for (NSDictionary *d in _daemons) {
        BOOL on = ![d[@"disabled"] boolValue];
        NSColor *dot = [d[@"running"] boolValue] ? rgb(0x4c, 0xae, 0x4f) : rgb(0xc9, 0xc2, 0xbb);

        [rows addObject:[self switchRow:d[@"what"] sub:d[@"label"] code:YES dot:dot on:on enabled:![d[@"locked"] boolValue] toggle:^(BOOL now) {
            PDSetDaemonDisabled(d[@"path"], !now);
            [self loadPane:@"services"];
            [self setNeedsDisplay:YES];
        }]];
    }
    if ([rows count] == 0)
        [rows addObject:[self valueRow:NSLocalizedString(@"No launch daemons found", @"") value:@""]];
    return [self card:rows at:y];
}

- (CGFloat)drawKextsAt:(CGFloat)y
{
    NSMutableArray *rows = [NSMutableArray array];
    DrawBlock (^grid)(NSArray *, BOOL) = ^DrawBlock(NSArray *cols, BOOL head) {
        return [[^(NSRect r) {
            CGFloat w = NSWidth(r) - 24, x = NSMinX(r) + 12;
            NSDictionary *a = head ? attrs(@"Inter-SemiBold", 11, rgb(0x9a, 0x8b, 0x7e)) : attrs(@"Inter-Regular", 12, rgb(0x4a, 0x4a, 0x4a));
            NSDictionary *code = attrs(@"SourceCodePro-Regular", 11.5, rgb(0x1b, 0x1b, 0x1b));
            CGFloat colX[3] = { x, x + w * 0.5, x + w * 0.75 };

            for (int i = 0; i < 3; i++) {
                NSDictionary *use = (!head && i == 0) ? code : a;
                CGFloat cx = colX[i];

                if (!head && i == 2) {
                    [([cols[2] isEqualToString:@"in use"] ? rgb(0x4c, 0xae, 0x4f) : rgb(0xc9, 0xc2, 0xbb)) setFill];
                    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(cx, NSMidY(r) - 4, 8, 8)] fill];
                    cx += 14;
                }
                [fitted(cols[i], use, w * (i == 0 ? 0.48 : 0.24)) drawAtPoint:NSMakePoint(cx, NSMidY(r) - 8) withAttributes:use];
            }
        } copy] autorelease];
    };

    y = [self lead:NSLocalizedString(@"Extensions on this system, and whether a driver of theirs is running.", @"") at:y];
    [rows addObject:[self row:30 draw:grid(@[ NSLocalizedString(@"Name", @""), NSLocalizedString(@"Family", @""), NSLocalizedString(@"State", @"") ], YES)]];
    for (NSDictionary *k in _kexts)
        [rows addObject:[self row:30 draw:grid(@[ k[@"name"], k[@"family"], k[@"state"] ], NO)]];
    return [self card:rows at:y];
}

- (NSString *)processState:(NSArray *)names
{
    NSSet *running = PDRunningProcesses();

    for (NSString *n in names) {
        if ([running containsObject:n])
            return @"running";
    }
    return NSLocalizedString(@"not running", @"");
}

- (CGFloat)drawSessionAt:(CGFloat)y
{
    return [self card:@[
        [self valueRow:@"Display server" value:@"Wayland"],
        [self valueRow:NSLocalizedString(@"Compositor", @"") value:[NSString stringWithFormat:NSLocalizedString(@"neuswc with pdwm, %@", @""), [self processState:@[ @"neuswc", @"pdwm", @"swc-launch" ]]]],
        [self valueRow:NSLocalizedString(@"Window server", @"") value:NSLocalizedString(@"AppKit over WindowServer", @"")],
        [self valueRow:NSLocalizedString(@"Menu bar", @"") value:[NSString stringWithFormat:NSLocalizedString(@"MenuBar, %@", @""), [self processState:@[ @"MenuBar" ]]]],
        [self valueRow:NSLocalizedString(@"Dock", @"") value:[NSString stringWithFormat:NSLocalizedString(@"Dock, %@", @""), [self processState:@[ @"Dock" ]]]],
    ] at:y];
}

- (CGFloat)drawDisplaysAt:(CGFloat)y
{
    CGDisplayModeRef current = CGDisplayCopyDisplayMode(CGMainDisplayID());
    size_t currentW = CGDisplayModeGetWidth(current), currentH = CGDisplayModeGetHeight(current);
    double currentHz = CGDisplayModeGetRefreshRate(current);
    NSMutableArray *sizes = [NSMutableArray array], *sizeNames = [NSMutableArray array];
    NSMutableArray *rates = [NSMutableArray array], *rateNames = [NSMutableArray array];
    CFIndex count = _displayModes != nil ? CFArrayGetCount((CFArrayRef)_displayModes) : 0;
    NSUInteger sizeIndex = 0, rateIndex = 0;

    CGDisplayModeRelease(current);
    // the array holds CGDisplayModeRefs, which are not objects
    for (CFIndex i = 0; i < count; i++) {
        CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex((CFArrayRef)_displayModes, i);
        NSValue *size = [NSValue valueWithSize:NSMakeSize(CGDisplayModeGetWidth(m), CGDisplayModeGetHeight(m))];
        NSNumber *hz = @(round(CGDisplayModeGetRefreshRate(m)));

        if (![sizes containsObject:size])
            [sizes addObject:size];
        if (CGDisplayModeGetWidth(m) == currentW && CGDisplayModeGetHeight(m) == currentH && ![rates containsObject:hz])
            [rates addObject:hz];
    }
    // largest first, as macOS lists them
    [sizes sortUsingComparator:^NSComparisonResult(NSValue *x, NSValue *y) {
        CGFloat ax = [x sizeValue].width * [x sizeValue].height, ay = [y sizeValue].width * [y sizeValue].height;
        return ax > ay ? NSOrderedAscending : ax < ay ? NSOrderedDescending : NSOrderedSame;
    }];
    [rates sortUsingComparator:^NSComparisonResult(NSNumber *x, NSNumber *y) { return [y compare:x]; }];
    for (NSValue *v in sizes) {
        NSSize sz = [v sizeValue];

        if (sz.width == currentW && sz.height == currentH)
            sizeIndex = [sizeNames count];
        [sizeNames addObject:[NSString stringWithFormat:NSLocalizedString(@"%1$.0f × %2$.0f", @""), sz.width, sz.height]];
    }
    for (NSNumber *hz in rates) {
        if ([hz doubleValue] == round(currentHz))
            rateIndex = [rateNames count];
        [rateNames addObject:[NSString stringWithFormat:NSLocalizedString(@"%@ Hz", @""), hz]];
    }

    // the mode with this size and rate, or the fastest one at this size
    CGDisplayModeRef (^find)(NSSize, double) = ^CGDisplayModeRef(NSSize sz, double hz) {
        CGDisplayModeRef best = NULL;

        for (CFIndex i = 0; i < count; i++) {
            CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex((CFArrayRef)_displayModes, i);

            if (CGDisplayModeGetWidth(m) != sz.width || CGDisplayModeGetHeight(m) != sz.height)
                continue;
            if (round(CGDisplayModeGetRefreshRate(m)) == round(hz))
                return m;
            if (best == NULL || CGDisplayModeGetRefreshRate(m) > CGDisplayModeGetRefreshRate(best))
                best = m;
        }
        return best;
    };
    void (^apply)(CGDisplayModeRef) = ^(CGDisplayModeRef mode) {
        if (mode != NULL && CGDisplaySetDisplayMode(CGMainDisplayID(), mode, NULL) != kCGErrorSuccess)
            [self setMessage:NSLocalizedString(@"The compositor did not accept that resolution.", @"")];
        [self setNeedsDisplay:YES];
    };

    if (count == 0) {
        return [self card:@[
            [self valueRow:NSLocalizedString(@"Display", @"") value:PDGraphics()],
            [self valueRow:NSLocalizedString(@"Resolution", @"") value:[NSString stringWithFormat:NSLocalizedString(@"%zu × %zu", @""), currentW, currentH]],
        ] at:y];
    }
    // five sizes from Larger Text to More Space, each a little desktop drawn at that mode's scale
    NSMutableArray *previews = [NSMutableArray array];
    NSUInteger shown = MIN([sizes count], 5);

    for (NSUInteger k = 0; k < shown; k++) {
        NSUInteger i = shown < 2 ? 0 : ([sizes count] - 1) - k * ([sizes count] - 1) / (shown - 1);

        if (![previews containsObject:sizes[i]])
            [previews addObject:sizes[i]];
    }
    if (![previews containsObject:sizes[sizeIndex]] && [previews count] > 0) {
        [previews replaceObjectAtIndex:[previews count] / 2 withObject:sizes[sizeIndex]];
        [previews sortUsingComparator:^NSComparisonResult(NSValue *x, NSValue *y) {
            CGFloat ax = [x sizeValue].width * [x sizeValue].height, ay = [y sizeValue].width * [y sizeValue].height;
            return ax < ay ? NSOrderedAscending : ax > ay ? NSOrderedDescending : NSOrderedSame;
        }];
    }

    y = [self card:@[
        [self row:132 draw:^(NSRect r) {
            NSUInteger n = [previews count];
            CGFloat tileW = 96, gap = floor((NSWidth(r) - 24 - n * tileW) / MAX(n, 1));
            NSDictionary *a = attrs(@"Inter-Regular", 11, rgb(0x4a, 0x4a, 0x4a));

            for (NSUInteger k = 0; k < n; k++) {
                NSSize mode = [previews[k] sizeValue];
                CGFloat tileH = round(tileW * mode.height / mode.width), f = tileW / mode.width;
                NSRect t = NSMakeRect(NSMinX(r) + 12 + gap / 2 + k * (tileW + gap), NSMinY(r) + 14 + (60 - tileH) / 2, tileW, tileH);
                BOOL on = mode.width == currentW && mode.height == currentH;
                NSRect win = NSMakeRect(NSMinX(t) + 40 * f, NSMinY(t) + 70 * f, MIN(700 * f, tileW - 8), MIN(450 * f, tileH - 10));
                NSRect dock = NSMakeRect(NSMidX(t) - 200 * f, NSMaxY(t) - 70 * f, 400 * f, 60 * f);
                NSString *name = [NSString stringWithFormat:NSLocalizedString(@"%1$.0f × %2$.0f", @""), mode.width, mode.height];
                NSSize ns = [name sizeWithAttributes:a];

                if (on) {
                    [accent() setFill];
                    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(t, -3, -3) xRadius:6 yRadius:6] fill];
                }
                [NSGraphicsContext saveGraphicsState];
                [[NSBezierPath bezierPathWithRoundedRect:t xRadius:4 yRadius:4] addClip];
                [[[[NSGradient alloc] initWithStartingColor:rgb(0xe8, 0x9a, 0x6a) endingColor:rgb(0xdc, 0x5f, 0x62)] autorelease] drawInRect:t angle:-30];
                [[NSColor colorWithCalibratedWhite:1 alpha:0.85] setFill];
                NSRectFill(NSMakeRect(NSMinX(t), NSMinY(t), tileW, MAX(24 * f, 1.5)));
                [[NSColor colorWithCalibratedWhite:0 alpha:0.15] setFill];
                [[NSBezierPath bezierPathWithRoundedRect:NSOffsetRect(win, 0, 0.5) xRadius:2 yRadius:2] fill];
                [fixed(0xf4, 0xf1, 0xee) setFill];
                [[NSBezierPath bezierPathWithRoundedRect:win xRadius:2 yRadius:2] fill];
                [[NSColor whiteColor] setFill];
                NSRectFill(NSMakeRect(NSMinX(win), NSMinY(win), NSWidth(win), MAX(38 * f, 1.5)));
                [[NSColor colorWithCalibratedWhite:1 alpha:0.6] setFill];
                [[NSBezierPath bezierPathWithRoundedRect:dock xRadius:2 yRadius:2] fill];
                [NSGraphicsContext restoreGraphicsState];
                [name drawAtPoint:NSMakePoint(NSMidX(t) - ns.width / 2, NSMinY(r) + 84) withAttributes:a];
                if (k == 0 || k == n - 1) {
                    NSString *hint = k == 0 ? NSLocalizedString(@"Larger Text", @"") : NSLocalizedString(@"More Space", @"");
                    NSDictionary *h = attrs(@"Inter-Regular", 10, rgb(0x8a, 0x8a, 0x8a));
                    NSSize hs = [hint sizeWithAttributes:h];

                    [hint drawAtPoint:NSMakePoint(NSMidX(t) - hs.width / 2, NSMinY(r) + 100) withAttributes:h];
                }
                [self hit:NSMakeRect(NSMinX(t) - 4, NSMinY(r), tileW + 8, NSHeight(r)) block:^(NSPoint p, BOOL dragging) {
                    if (dragging || on)
                        return;
                    apply(find(mode, currentHz));
                }];
            }
        }],
    ] at:y];

    return [self card:@[
        [self valueRow:NSLocalizedString(@"Display", @"") value:PDGraphics()],
        [self popupRow:NSLocalizedString(@"Resolution", @"") items:sizeNames selected:sizeIndex pick:^(NSUInteger i) {
            apply(find([sizes[i] sizeValue], currentHz));
        }],
        [self popupRow:NSLocalizedString(@"Refresh rate", @"") items:rateNames selected:rateIndex pick:^(NSUInteger i) {
            apply(find(NSMakeSize(currentW, currentH), [rates[i] doubleValue]));
        }],
    ] at:y];
}

// a link or a small button at the right of a row, laid out right to left
- (CGFloat)wifiControl:(NSString *)title button:(BOOL)button right:(CGFloat)right mid:(CGFloat)mid action:(void (^)(void))action
{
    NSDictionary *a = attrs(@"Inter-Regular", 12, button ? rgb(0x1b, 0x1b, 0x1b) : accent());
    NSSize s = [title sizeWithAttributes:a];
    NSRect b = button ? NSMakeRect(right - s.width - 20, mid - 11, s.width + 20, 22) : NSMakeRect(right - s.width, mid - 9, s.width, 18);

    if (button) {
        [[NSColor colorWithCalibratedWhite:0 alpha:0.15] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, -0.5, -0.5) xRadius:6.5 yRadius:6.5] fill];
        [(darkNow() ? fixed(0x57, 0x51, 0x53) : [NSColor whiteColor]) setFill];
        [[NSBezierPath bezierPathWithRoundedRect:b xRadius:6 yRadius:6] fill];
    }
    [title drawAtPoint:NSMakePoint(NSMinX(b) + (button ? 10 : 0), mid - s.height / 2) withAttributes:a];
    [self hit:NSInsetRect(b, -3, -4) block:^(NSPoint p, BOOL dragging) {
        if (!dragging)
            action();
    }];
    return NSMinX(b) - 12;
}

// one network: the glyph in its circle, the name with a line under it, and what can be done with it
- (NSDictionary *)wifiRow:(NSString *)name sub:(NSString *)sub level:(int)level on:(BOOL)on lock:(BOOL)lock
                  actions:(NSArray *)actions
{
    return [self row:(sub != nil ? 46 : ROW_MIN) draw:^(NSRect r) {
        NSRect circle = NSMakeRect(NSMinX(r) + 12, NSMidY(r) - 12, 24, 24);
        CGFloat right = NSMaxX(r) - 12, x = NSMinX(r) + 46;
        PDWiFi *w = [PDWiFi shared];

        [(on ? accent() : rgba(127, 127, 127, 0.16)) setFill];
        [[NSBezierPath bezierPathWithOvalInRect:circle] fill];
        PDDrawWiFiGlyph(NSInsetRect(circle, 5, 5), level, on ? [NSColor whiteColor] : rgb(0x1b, 0x1b, 0x1b), YES);
        for (NSArray *act in [actions reverseObjectEnumerator])
            right = [self wifiControl:act[0] button:[act[1] boolValue] right:right mid:NSMidY(r) action:act[2]];
        if ([w.busy isEqualToString:name]) {
            PDDrawWiFiSpinner(NSMakeRect(right - 12, NSMidY(r) - 7, 14, 14), accent());
            [self setNeedsDisplay:YES];
        }
        [self drawLabel:name sub:sub in:NSMakeRect(x, NSMinY(r), right - x - 30, NSHeight(r)) code:NO];
        if (lock) {
            NSDictionary *a = attrs(@"Inter-Regular", 13, rgb(0x1b, 0x1b, 0x1b));
            CGFloat lx = x + MIN([name sizeWithAttributes:a].width, right - x - 40) + 7;
            PDDrawWiFiLock(NSMakeRect(lx, NSMidY(r) - (sub ? 13 : 6), 10, 12), rgba(127, 127, 127, 0.8), YES);
        }
    }];
}

// the demo's Wi-Fi pane: the switch, the joined network and its details, known and other networks, then Ethernet
- (CGFloat)drawNetworkAt:(CGFloat)y
{
    PDWiFi *w = [PDWiFi shared];
    PDWiFiNetwork *cur = w.current;
    NSColor *tint = accent();
    NSMutableArray *rows = [NSMutableArray array];
    NSArray *ifs = PDNetworkInterfaces();
    NSString *wifiIP = nil;
    NSMutableArray *eth = [NSMutableArray array];

    for (NSArray *i in ifs) {
        if (w.interface && [i[0] isEqualToString:w.interface])
            wifiIP = [[i[1] componentsSeparatedByString:@" "] firstObject];
        else
            [eth addObject:i];
    }

    if (!w.hardware) {
        y = [self card:@[ [self valueRow:@"Wi-Fi" value:NSLocalizedString(@"No Wi-Fi hardware", @"")] ] at:y];
    } else {
        NSString *state = !w.powered ? NSLocalizedString(@"Off", @"") : cur ? [NSString stringWithFormat:NSLocalizedString(@"Connected to %@", @""), cur.ssid]
                        : w.busy ? [NSString stringWithFormat:NSLocalizedString(@"Joining %@…", @""), w.busy] : w.lastError ?: NSLocalizedString(@"Not connected", @"");
        [rows addObject:[self switchRow:@"Wi-Fi" sub:state code:NO dot:nil on:w.powered enabled:YES toggle:^(BOOL on) {
            [w setPower:on];
        }]];
        if (cur) {
            NSString *sub = cur.security ? [NSString stringWithFormat:NSLocalizedString(@"Connected · %@ Personal", @""), cur.security] : NSLocalizedString(@"Connected", @"");
            [rows addObject:[self wifiRow:cur.ssid sub:sub level:[cur bars] on:YES lock:NO actions:@[
                @[ _wifiDetails ? NSLocalizedString(@"Hide Details", @"") : NSLocalizedString(@"Details…", @""), @NO, ^{ _wifiDetails = !_wifiDetails; [self setNeedsDisplay:YES]; } ],
                @[ NSLocalizedString(@"Disconnect", @""), @YES, ^{ [w leave]; } ],
            ]]];
            if (_wifiDetails) {
                NSArray *facts = @[
                    @[ NSLocalizedString(@"IP address", @""), wifiIP ?: NSLocalizedString(@"Waiting for DHCP", @"") ],
                    @[ NSLocalizedString(@"Security", @""), cur.security ? [cur.security stringByAppendingString:NSLocalizedString(@" Personal", @"")] : NSLocalizedString(@"None", @"") ],
                    @[ NSLocalizedString(@"Channel", @""), [NSString stringWithFormat:NSLocalizedString(@"%1$u (%2$@ GHz)", @""), cur.channel, cur.channel > 14 ? @"5" : @"2.4"] ],
                    @[ NSLocalizedString(@"Signal", @""), [NSString stringWithFormat:NSLocalizedString(@"%d dBm", @""), cur.rssi] ],
                    @[ NSLocalizedString(@"Wi-Fi address", @""), w.hardwareAddress ?: @"" ],
                    @[ NSLocalizedString(@"Interface", @""), w.interface ?: @"" ],
                ];
                [rows addObject:[self row:[facts count] * 19 + 14 draw:^(NSRect r) {
                    CGFloat fy = NSMinY(r) + 8;
                    for (NSArray *f in facts) {
                        [f[0] drawAtPoint:NSMakePoint(NSMinX(r) + 46, fy) withAttributes:attrs(@"Inter-Regular", 12, rgb(0x8a, 0x8a, 0x8a))];
                        [f[1] drawAtPoint:NSMakePoint(NSMinX(r) + 176, fy) withAttributes:attrs(@"Inter-Regular", 12, rgb(0x1b, 0x1b, 0x1b))];
                        fy += 19;
                    }
                }]];
            }
        }
        y = [self card:rows at:y];

        if (w.powered) {
            NSMutableArray *known = [NSMutableArray array], *other = [NSMutableArray array];
            NSMutableDictionary *inRange = [NSMutableDictionary dictionary];

            for (PDWiFiNetwork *n in w.inRange)
                inRange[n.ssid] = n;
            for (NSString *k in w.known) {
                PDWiFiNetwork *n = inRange[k];
                NSMutableArray *acts = [NSMutableArray arrayWithObject:@[ NSLocalizedString(@"Forget", @""), @NO, ^{ [w forget:k]; } ]];
                if (cur && [cur.ssid isEqualToString:k])
                    continue;
                if (n)
                    [acts addObject:@[ NSLocalizedString(@"Join", @""), @YES, ^{ [w pick:n accent:tint]; } ]];
                [known addObject:[self wifiRow:k sub:n ? NSLocalizedString(@"In range", @"") : NSLocalizedString(@"Not in range", @"") level:n ? [n bars] : 0 on:NO lock:NO actions:acts]];
            }
            if ([known count]) {
                y = [self title:NSLocalizedString(@"Known Networks", @"") at:y];
                y = [self card:known at:y];
            }
            for (PDWiFiNetwork *n in w.inRange) {
                if (n.known || (cur && [cur.ssid isEqualToString:n.ssid]))
                    continue;
                [other addObject:[self wifiRow:n.ssid sub:nil level:[n bars] on:NO lock:n.security != nil actions:@[
                    @[ NSLocalizedString(@"Join", @""), @YES, ^{ [w pick:n accent:tint]; } ],
                ]]];
            }
            if ([other count] == 0)
                [other addObject:[self valueRow:w.scanning ? NSLocalizedString(@"Looking for networks…", @"") : NSLocalizedString(@"No other networks in range", @"") value:@""]];
            [other addObject:[self row:ROW_MIN draw:^(NSRect r) {
                [self wifiControl:NSLocalizedString(@"Other Network…", @"") button:NO right:NSMinX(r) + 12 + [NSLocalizedString(@"Other Network…", @"") sizeWithAttributes:attrs(@"Inter-Regular", 12, accent())].width
                              mid:NSMidY(r) action:^{ [w joinOther:tint]; }];
            }]];
            y = [self title:NSLocalizedString(@"Other Networks", @"") at:y];
            y = [self card:other at:y];
        }
    }

    NSMutableArray *er = [NSMutableArray array];
    NSArray *dns = PDNameServers();
    for (NSArray *i in eth)
        [er addObject:[self valueRow:i[0] value:i[1]]];
    if ([er count] == 0)
        [er addObject:[self valueRow:NSLocalizedString(@"Ethernet", @"") value:NSLocalizedString(@"No cable connected", @"")]];
    [er addObject:[self valueRow:@"DNS" value:[dns count] > 0 ? [dns componentsJoinedByString:@", "] : @"mDNSResponder"]];
    y = [self title:NSLocalizedString(@"Ethernet", @"") at:y];
    return [self card:er at:y];
}

- (CGFloat)drawSoundAt:(CGFloat)y
{
    NSMutableArray *rows = [NSMutableArray array];

    for (NSArray *d in PDAudioDevices())
        [rows addObject:[self valueRow:d[0] value:d[1]]];
    if ([rows count] == 0)
        [rows addObject:[self valueRow:NSLocalizedString(@"Output", @"") value:NSLocalizedString(@"No audio device", @"")]];
    return [self card:rows at:y];
}

- (CGFloat)drawPowerAt:(CGFloat)y
{
    NSDictionary *b = PDBattery();

    return [self card:@[
        [self valueRow:NSLocalizedString(@"Battery", @"") value:b != nil ? [NSString stringWithFormat:@"%@%%%@", b[@"percent"], [b[@"charging"] boolValue] ? NSLocalizedString(@", charging", @"") : @""]
                                              : NSLocalizedString(@"None, on AC power", @"")],
        [self valueRow:NSLocalizedString(@"Sleep after", @"") value:NSLocalizedString(@"Never", @"")],
    ] at:y];
}

- (CGFloat)drawInputAt:(CGFloat)y
{
    NSMutableArray *rows = [NSMutableArray array];

    for (NSArray *d in PDInputDevices())
        [rows addObject:[self valueRow:d[0] value:d[1]]];
    [rows addObject:[self valueRow:NSLocalizedString(@"Keyboard layout", @"") value:NSLocalizedString(@"U.S.", @"")]];
    return [self card:rows at:y];
}

- (CGFloat)drawClockAt:(CGFloat)y
{
    // until the switch is used the clock follows the locale, so it shows the locale's hour cycle
    NSString *localeTime = [NSDateFormatter dateFormatFromTemplate:@"jmm" options:0 locale:nil];
    id clock24 = PDPref(PDMenuBarDomain, @"Clock24");
    BOOL h24 = clock24 != nil ? [clock24 boolValue] : [localeTime rangeOfString:@"H"].location != NSNotFound;
    BOOL iso = [PDPref(PDMenuBarDomain, @"ISO8601") boolValue];
    BOOL date = PDPref(PDMenuBarDomain, @"ShowDate") ? [PDPref(PDMenuBarDomain, @"ShowDate") boolValue] : YES;
    NSDateFormatter *f = [[[NSDateFormatter alloc] init] autorelease];

    if (iso)
        [f setDateFormat:@"yyyy-MM-dd'T'HH:mm:ssZZZZZ"];
    else
        [f setLocalizedDateFormatFromTemplate:h24 ? @"EEEEdMMMMyyyyHHmm" : @"EEEEdMMMMyyyyhmma"];
    return [self card:@[
        [self switchRow:NSLocalizedString(@"ISO 8601", @"") sub:NSLocalizedString(@"Dates as 2026-10-10, times as 14:05", @"") code:NO dot:nil on:iso enabled:YES toggle:^(BOOL on) {
            PDSetPref(PDMenuBarDomain, @"ISO8601", @(on));
            [self setNeedsDisplay:YES];
        }],
        [self switchRow:NSLocalizedString(@"24-hour clock", @"") sub:nil code:NO dot:nil on:h24 || iso enabled:!iso toggle:^(BOOL on) {
            PDSetPref(PDMenuBarDomain, @"Clock24", @(on));
            [self setNeedsDisplay:YES];
        }],
        [self switchRow:NSLocalizedString(@"Date in the menu bar", @"") sub:nil code:NO dot:nil on:date enabled:YES toggle:^(BOOL on) {
            PDSetPref(PDMenuBarDomain, @"ShowDate", @(on));
            [self setNeedsDisplay:YES];
        }],
        [self valueRow:NSLocalizedString(@"Time zone", @"") value:PDTimeZone()],
        [self valueRow:NSLocalizedString(@"Now", @"") value:[f stringFromDate:[NSDate date]]],
    ] at:y];
}

- (CGFloat)drawAboutAt:(CGFloat)y
{
    return [self card:@[
        [self valueRow:NSLocalizedString(@"Version", @"") value:[NSString stringWithFormat:NSLocalizedString(@"PureDarwin %1$@ (%2$@)", @""), PDSystemVersion(), PDBuildVersion()]],
        [self valueRow:NSLocalizedString(@"Kernel", @"") value:[NSString stringWithFormat:@"%@, %@", PDKernelVersion(), PDXNUTag()]],
        [self valueRow:NSLocalizedString(@"Processor", @"") value:PDProcessor()],
        [self valueRow:NSLocalizedString(@"Memory", @"") value:PDMemory()],
        [self valueRow:NSLocalizedString(@"Graphics", @"") value:PDGraphics()],
        [self valueRow:@"Source" value:@"github.com/PureDarwin"],
        [self buttonRow:NSLocalizedString(@"System Report…", @"") note:nil action:^{
            [[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/Utilities/System Information.app"];
        }],
    ] at:y];
}

- (CGFloat)drawPaneAt:(CGFloat)y
{
    y += 18;
    if ([_pane isEqualToString:@"appearance"])
        return [self drawAppearanceAt:y];
    if ([_pane isEqualToString:@"users"])
        return [self drawAccountAt:y];
    if ([_pane isEqualToString:@"boot"])
        return [self drawBootAt:y];
    if ([_pane isEqualToString:@"services"])
        return [self drawServicesAt:y];
    if ([_pane isEqualToString:@"kexts"])
        return [self drawKextsAt:y];
    if ([_pane isEqualToString:@"session"])
        return [self drawSessionAt:y];
    if ([_pane isEqualToString:@"displays"])
        return [self drawDisplaysAt:y];
    if ([_pane isEqualToString:@"network"])
        return [self drawNetworkAt:y];
    if ([_pane isEqualToString:@"sound"])
        return [self drawSoundAt:y];
    if ([_pane isEqualToString:@"power"])
        return [self drawPowerAt:y];
    if ([_pane isEqualToString:@"input"])
        return [self drawInputAt:y];
    if ([_pane isEqualToString:@"clock"])
        return [self drawClockAt:y];
    return [self drawAboutAt:y];
}

- (NSRect)backRect
{
    return NSMakeRect(84, (TB_H - 22) / 2, 24, 22);
}

- (NSRect)searchRect
{
    return NSMakeRect(NSWidth([self bounds]) - 10 - 180, (TB_H - 24) / 2, 180, 24);
}

- (NSRect)crumbHomeRect
{
    NSSize s = [NSLocalizedString(@"Settings", @"") sizeWithAttributes:attrs(@"Inter-SemiBold", 13, rgb(0x1b, 0x1b, 0x1b))];

    return NSMakeRect(NSMaxX([self backRect]) + 8, (TB_H - 20) / 2, s.width, 20);
}

- (void)drawToolbar
{
    NSRect b = [self bounds], search = [self searchRect], back = [self backRect];
    NSArray *info = paneInfo(_pane);
    NSDictionary *crumb = attrs(@"Inter-SemiBold", 13, info != nil ? rgb(0x8a, 0x7d, 0x72) : rgb(0x1b, 0x1b, 0x1b));
    NSDictionary *crumbOn = attrs(@"Inter-SemiBold", 13, rgb(0x1b, 0x1b, 0x1b));
    NSDictionary *field = attrs(@"Inter-Regular", 12, [_search length] > 0 ? rgb(0x1b, 0x1b, 0x1b) : rgb(0x8a, 0x8a, 0x8a));
    NSString *placeholder = [_search length] > 0 ? _search : NSLocalizedString(@"Search settings", @"");
    CGFloat x = NSMinX([self crumbHomeRect]);
    NSBezierPath *chev = [NSBezierPath bezierPath];
    NSSize s;
    int row;

    for (row = 0; row < TB_H; row++) {
        CGFloat top = darkNow() ? 0x33 : 0xf7, idle = darkNow() ? 0x2c : 0xf3;

        [[NSColor colorWithCalibratedWhite:([[self window] isKeyWindow] ? top - (darkNow() ? 7.0 : 10.0) * row / (TB_H - 1) : idle) / 255.0 alpha:1] setFill];
        NSRectFill(NSMakeRect(0, row, NSWidth(b), 1));
    }
    [rgb(0xdc, 0xd6, 0xd0) setFill];
    NSRectFill(NSMakeRect(0, TB_H, NSWidth(b), 1));

    [chev moveToPoint:NSMakePoint(NSMidX(back) + 2.5, NSMidY(back) - 5.5)];
    [chev lineToPoint:NSMakePoint(NSMidX(back) - 3, NSMidY(back))];
    [chev lineToPoint:NSMakePoint(NSMidX(back) + 2.5, NSMidY(back) + 5.5)];
    [chev setLineWidth:1.7];
    [([_history count] > 0 ? rgb(0x55, 0x55, 0x55) : rgb(0xc4, 0xc4, 0xc4)) setStroke];
    [chev stroke];

    s = [NSLocalizedString(@"Settings", @"") sizeWithAttributes:crumb];
    [NSLocalizedString(@"Settings", @"") drawAtPoint:NSMakePoint(x, floor((TB_H - s.height) / 2)) withAttributes:crumb];
    if (info != nil) {
        x += s.width + 6;
        [@"›" drawAtPoint:NSMakePoint(x, floor((TB_H - s.height) / 2)) withAttributes:attrs(@"Inter-SemiBold", 13, rgb(0xb9, 0xae, 0xa4))];
        x += 12;
        [info[1] drawAtPoint:NSMakePoint(x, floor((TB_H - s.height) / 2)) withAttributes:crumbOn];
    }

    [[NSColor colorWithCalibratedWhite:0 alpha:_searchActive ? 0.0 : 0.1] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(search, -1, -1) xRadius:7 yRadius:7] fill];
    if (_searchActive) {
        [[accent() colorWithAlphaComponent:0.6] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(search, -2.5, -2.5) xRadius:8 yRadius:8] fill];
    }
    [[NSColor colorWithCalibratedWhite:1 alpha:darkNow() ? 0.08 : 0.9] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:search xRadius:6 yRadius:6] fill];
    [image(@"search") drawInRect:NSMakeRect(NSMinX(search) + 8, NSMidY(search) - 6, 12, 12) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:0.55 respectFlipped:YES hints:nil];
    s = [placeholder sizeWithAttributes:field];
    [fitted(placeholder, field, NSWidth(search) - 34) drawAtPoint:NSMakePoint(NSMinX(search) + 26, NSMidY(search) - s.height / 2) withAttributes:field];
    if (_searchActive) {
        CGFloat cx = NSMinX(search) + 26 + ([_search length] > 0 ? MIN(s.width, NSWidth(search) - 34) : 0) + 1;

        [rgb(0x1b, 0x1b, 0x1b) setFill];
        NSRectFill(NSMakeRect(cx, NSMidY(search) - 7, 1, 14));
    }
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    CGFloat y;
    // PD_SETTINGS_TRACE=1 logs how long each full draw takes
    static int trace = -1;
    NSDate *started = [NSDate date];

    if (trace < 0)
        trace = getenv("PD_SETTINGS_TRACE") != NULL;

    _dirty = dirty;
    // a banner-only redraw keeps the tiles' click targets, it does not draw them again
    if (!([_pane isEqualToString:@"home"] && [_search length] == 0 && NSMaxY(dirty) <= TB_H + 1 - _scroll + BANNER_H))
        [_hits removeAllObjects];
    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:8 yRadius:8] addClip];
    [rgb(0xf4, 0xf1, 0xee) setFill];
    NSRectFill(b);

    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:NSMakeRect(0, TB_H + 1, NSWidth(b), NSHeight(b) - TB_H - 1)];
    y = TB_H + 1 - _scroll;
    y = [_pane isEqualToString:@"home"] ? [self drawHomeAt:y] : [self drawPaneAt:y];
    _contentHeight = y + _scroll - TB_H + 10;
    if (_contentHeight > NSHeight(b) - TB_H) {
        CGFloat view = NSHeight(b) - TB_H - 1, h = MAX(24, view * view / _contentHeight);
        CGFloat top = TB_H + 1 + (view - h) * _scroll / (_contentHeight - view);

        CGFloat content = _contentHeight;

        [[NSColor colorWithCalibratedWhite:0 alpha:0.28] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSWidth(b) - 8, top + 2, 5, h - 4) xRadius:2.5 yRadius:2.5] fill];
        // the knob follows the pointer from where it was grabbed, a press on the track jumps it there
        [self hit:NSMakeRect(NSWidth(b) - 14, TB_H + 1, 14, view) block:^(NSPoint p, BOOL dragging) {
            static CGFloat grab;

            if ([[NSApp currentEvent] type] == NSLeftMouseDown)
                grab = (p.y >= top && p.y < top + h) ? p.y - top : h / 2;
            _scroll = MIN(MAX((p.y - grab - TB_H - 1) / (view - h) * (content - view), 0), content - view);
            [self setNeedsDisplay:YES];
        }];
    }
    [NSGraphicsContext restoreGraphicsState];
    [self drawToolbar];
    if (trace)
        NSLog(@"Settings: draw %@ %.1f ms (banner %.1f cards %.1f glyphs %.1f text %.1f)", _pane, -[started timeIntervalSinceNow] * 1000,
              tBanner, tCard, tGlyph, tText);
    tBanner = tCard = tGlyph = tText = 0;
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (p.y < TB_H) {
        if (NSPointInRect(p, [self backRect]))
            [self goBack:nil];
        else if (NSPointInRect(p, [self searchRect]))
            [self find:nil];
        else if (paneInfo(_pane) != nil && NSPointInRect(p, [self crumbHomeRect]))
            [self goHome:nil];
        else
            [[self window] performWindowDragWithEvent:event];
        return;
    }
    if (_searchActive && [_search length] == 0) {
        _searchActive = NO;
        [self setNeedsDisplay:YES];
    }
    for (SettingsHit *h in [_hits reverseObjectEnumerator]) {
        if (NSPointInRect(p, h->rect)) {
            [_drag release];
            _drag = [h->block retain];
            h->block(p, YES);
            return;
        }
    }
}

// a held control follows the pointer, the release commits it
- (void)mouseDragged:(NSEvent *)event
{
    if (_drag != nil)
        ((HitBlock)_drag)([self convertPoint:[event locationInWindow] fromView:nil], YES);
}

- (void)mouseUp:(NSEvent *)event
{
    HitBlock drag = [[_drag retain] autorelease];

    [_drag release];
    _drag = nil;
    if (drag != nil)
        drag([self convertPoint:[event locationInWindow] fromView:nil], NO);
}

- (void)scrollWheel:(NSEvent *)event
{
    CGFloat max = MAX(0, _contentHeight - (NSHeight([self bounds]) - TB_H - 1));

    _scroll = MIN(MAX(_scroll - [event deltaY] * LINE_SCROLL, 0), max);
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event characters];
    unichar c = [chars length] > 0 ? [chars characterAtIndex:0] : 0;

    if (!_searchActive || ([event modifierFlags] & NSCommandKeyMask)) {
        [super keyDown:event];
        return;
    }
    if (c == 27) {
        [_search setString:@""];
        _searchActive = NO;
    } else if (c == 127 || c == 8) {
        if ([_search length] > 0)
            [_search deleteCharactersInRange:NSMakeRange([_search length] - 1, 1)];
    } else if (c >= 32 && c < 0xf700) {
        [_search appendString:chars];
    } else {
        return;
    }
    // typing searches from the home tiles, as in the demo
    if (![_pane isEqualToString:@"home"]) {
        [_pane release];
        _pane = @"home";
        _scroll = 0;
    }
    [self setNeedsDisplay:YES];
}

@end
