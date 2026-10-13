/*
 * MenuBarView.m - draws the bar and pops up the menus.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "MenuBarView.h"
#import "WiFiMenu.h"
#import "PDWiFiUI.h"
#import "AboutPanel.h"
#include <math.h>

@interface NSMenu (PDCancelTracking)
+ (void)_pdSetTrackingExitRects:(NSArray *)rects;
+ (NSInteger)_pdTrackingExitIndex;
@end

static const CGFloat ItemPad = 9;
static const CGFloat IconSize = 16;
static const CGFloat FontSize = 13;

// the beak menu is index -1, app menus count from 0
enum { BeakIndex = -1, NoMenu = -2 };

@interface MenuBarView (Lookout)
- (void)openLookout:(id)sender;
- (void)searchLookout:(id)sender;
- (void)launchLookoutResult:(id)sender;
- (void)closeLookout;
- (void)layoutLookout;
- (void)controlTextDidChange:(NSNotification *)note;
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)selector;
@end

@interface NSWindow (PDLookoutBlur)
- (void)_setBackgroundBlurInset:(CGFloat)inset radius:(CGFloat)radius;
@end

// Lookout, laid out as the demo: a 680pt bar, then a grouped list on the left and a preview on the right
#define LookoutW 680
#define LookoutBarH 52
#define LookoutResH 320
#define LookoutListW 313
#define LookoutRowH 28
#define LookoutHeadH 24
#define LookoutRadius 14

static NSColor *lookoutRGB(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static BOOL darkNow(void)
{
    return [[NSAppearance currentDrawingAppearance] _isDark];
}

// one colour in the light appearance, another in the dark
static NSColor *lightDark(NSColor *light, NSColor *dark)
{
    return [NSColor colorWithName:nil dynamicProvider:^NSColor *(NSAppearance *a) {
        return [a _isDark] ? dark : light;
    }];
}

// the demo's #eee text on the dark panels
static NSColor *ink(void)
{
    return lightDark(lookoutRGB(0x1b, 0x1b, 0x1b, 1), lookoutRGB(0xee, 0xee, 0xee, 1));
}

// the icon's shape in the light ink, worked out on the pixels so it does not depend on compositing into an image's focus
static NSImage *lightTint(NSImage *image)
{
    static NSMutableDictionary *cache;
    NSValue *key = [NSValue valueWithNonretainedObject:image];
    NSImage *done = [cache objectForKey:key];

    if (done)
        return done;

    NSBitmapImageRep *src = [NSBitmapImageRep imageRepWithData:[image TIFFRepresentation]];
    if (!src || [src bitsPerSample] != 8 || [src samplesPerPixel] != 4 || [src isPlanar])
        return nil;

    NSInteger w = [src pixelsWide], h = [src pixelsHigh], row = [src bytesPerRow];
    BOOL alphaFirst = ([src bitmapFormat] & NSAlphaFirstBitmapFormat) != 0;
    BOOL premultiplied = ([src bitmapFormat] & NSAlphaNonpremultipliedBitmapFormat) == 0;
    NSBitmapImageRep *dst = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSCalibratedRGBColorSpace
        bitmapFormat:[src bitmapFormat] bytesPerRow:row bitsPerPixel:32] autorelease];
    if (!dst)
        return nil;

    const unsigned char *in = [src bitmapData];
    unsigned char *out = [dst bitmapData];

    for (NSInteger y = 0; y < h; y++) {
        for (NSInteger x = 0; x < w; x++) {
            const unsigned char *p = in + y * row + x * 4;
            unsigned char *q = out + y * row + x * 4;
            unsigned char a = alphaFirst ? p[0] : p[3];
            unsigned char ink = premultiplied ? (unsigned char)(a * 0xf0 / 255) : 0xf0;

            if (alphaFirst) {
                q[0] = a;
                q[1] = q[2] = q[3] = ink;
            } else {
                q[0] = q[1] = q[2] = ink;
                q[3] = a;
            }
        }
    }

    done = [[[NSImage alloc] initWithSize:[image size]] autorelease];
    [done addRepresentation:dst];
    if (!cache)
        cache = [[NSMutableDictionary alloc] init];
    [cache setObject:done forKey:key];
    return done;
}

// monochrome icons are drawn light on the dark bar
static void drawIcon(NSImage *image, NSRect rect)
{
    if (!darkNow()) {
        [image drawInRect:rect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1];
        return;
    }
    NSImage *tinted = lightTint(image);
    [(tinted ?: image) drawInRect:rect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1];
}

static NSFont *lookoutFont(NSString *name, CGFloat size)
{
    return [NSFont fontWithName:name size:size] ?: [NSFont systemFontOfSize:size];
}

static void lookoutCentred(NSString *text, NSDictionary *attrs, CGFloat midX, CGFloat y)
{
    NSSize size = [text sizeWithAttributes:attrs];

    [text drawAtPoint:NSMakePoint(floor(midX - size.width / 2), y) withAttributes:attrs];
}

// draws the bar, the list and the preview, the field is the one real control on it
@interface LookoutView : NSView {
@public
    NSArray *hits;
    NSInteger selection;
    NSImage *glass;
    NSTextField *field;
    MenuBarView *owner;
}
@end

@implementation LookoutView

- (BOOL)isFlipped
{
    return YES;
}

// rows and their group headers from the top of the list, a header comes first whenever the group changes
- (NSRect)rowRect:(NSInteger)index
{
    CGFloat y = LookoutBarH + 1 + 6;
    NSString *group = nil;

    for (NSInteger i = 0; i <= index && i < (NSInteger)[hits count]; i++) {
        if (![hits[i][@"group"] isEqualToString:group]) {
            group = hits[i][@"group"];
            y += LookoutHeadH;
        }
        if (i == index)
            return NSMakeRect(6, y, LookoutListW - 12, LookoutRowH);
        y += LookoutRowH;
    }
    return NSZeroRect;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSBezierPath *panel = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.25, 0.25) xRadius:LookoutRadius yRadius:LookoutRadius];
    NSDictionary *row = @{ NSFontAttributeName: lookoutFont(@"Inter-Regular", 13), NSForegroundColorAttributeName: ink() };
    NSDictionary *rowOn = @{ NSFontAttributeName: lookoutFont(@"Inter-Regular", 13), NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSDictionary *head = @{ NSFontAttributeName: lookoutFont(@"Inter-SemiBold", 11), NSForegroundColorAttributeName: lightDark(lookoutRGB(0x7a, 0x7a, 0x7a, 1), lookoutRGB(0x9a, 0x9a, 0x9a, 1)) };
    NSDictionary *grey = @{ NSFontAttributeName: lookoutFont(@"Inter-Regular", 12), NSForegroundColorAttributeName: lightDark(lookoutRGB(0x7a, 0x7a, 0x7a, 1), lookoutRGB(0x9a, 0x9a, 0x9a, 1)) };
    NSDictionary *selected = selection < (NSInteger)[hits count] ? hits[selection] : nil;
    NSString *group = nil;

    [[NSColor clearColor] set];
    NSRectFill(b);
    [(darkNow() ? lookoutRGB(44, 42, 44, 0.82) : lookoutRGB(240, 238, 236, 0.82)) setFill];
    [panel fill];
    [(darkNow() ? lookoutRGB(255, 255, 255, 0.12) : lookoutRGB(0, 0, 0, 0.25)) setStroke];
    [panel setLineWidth:0.5];
    [panel stroke];

    [glass drawInRect:NSMakeRect(16, (LookoutBarH - 22) / 2, 22, 22) fromRect:NSZeroRect operation:NSCompositeSourceOver
             fraction:0.6 respectFlipped:YES hints:nil];
    if (selected[@"icon"] != nil)
        [selected[@"icon"] drawInRect:NSMakeRect(LookoutW - 16 - 26, (LookoutBarH - 26) / 2, 26, 26) fromRect:NSZeroRect
                            operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    if ([hits count] == 0)
        return;

    // hairlines blend over the panel, NSRectFill would copy their alpha straight in
    [(darkNow() ? lookoutRGB(255, 255, 255, 0.1) : lookoutRGB(0, 0, 0, 0.1)) setFill];
    NSRectFillUsingOperation(NSMakeRect(0, LookoutBarH, LookoutW, 1), NSCompositeSourceOver);
    [(darkNow() ? lookoutRGB(255, 255, 255, 0.08) : lookoutRGB(0, 0, 0, 0.08)) setFill];
    NSRectFillUsingOperation(NSMakeRect(LookoutListW, LookoutBarH + 1, 1, LookoutResH), NSCompositeSourceOver);

    for (NSInteger i = 0; i < (NSInteger)[hits count]; i++) {
        NSDictionary *hit = hits[i];
        NSRect r = [self rowRect:i];
        BOOL on = i == selection;

        if (NSMaxY(r) > NSMaxY(b) - 6)
            break;
        if (![hit[@"group"] isEqualToString:group]) {
            group = hit[@"group"];
            [group drawAtPoint:NSMakePoint(14, NSMinY(r) - LookoutHeadH + 8) withAttributes:head];
        }
        if (on) {
            [[NSColor controlAccentColor] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6] fill];
        }
        [hit[@"icon"] drawInRect:NSMakeRect(NSMinX(r) + 8, NSMidY(r) - 10, 20, 20) fromRect:NSZeroRect
                       operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        [hit[@"name"] drawAtPoint:NSMakePoint(NSMinX(r) + 36, NSMidY(r) - 8) withAttributes:on ? rowOn : row];
    }

    // the preview pane: the big icon, or the answer of a sum
    CGFloat mid = LookoutListW + (LookoutW - LookoutListW) / 2, centre = LookoutBarH + 1 + LookoutResH / 2;

    if (selected[@"answer"] != nil) {
        lookoutCentred(selected[@"answer"], @{ NSFontAttributeName: lookoutFont(@"Inter-Light", 40),
                                            NSForegroundColorAttributeName: ink() }, mid, centre - 40);
        lookoutCentred([field stringValue], grey, mid, centre + 12);
        lookoutCentred(NSLocalizedString(@"Return copies the result", @""), grey, mid, centre + 32);
        return;
    }
    [selected[@"icon"] drawInRect:NSMakeRect(mid - 48, centre - 80, 96, 96) fromRect:NSZeroRect
                        operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    lookoutCentred(selected[@"name"], @{ NSFontAttributeName: lookoutFont(@"Inter-Bold", 17),
                                      NSForegroundColorAttributeName: ink() }, mid, centre + 26);
    lookoutCentred(selected[@"path"] ? [NSString stringWithFormat:@"%@ · %@", selected[@"kind"],
                                     [selected[@"path"] stringByAbbreviatingWithTildeInPath]] : selected[@"kind"],
                grey, mid, centre + 52);
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    for (NSInteger i = 0; i < (NSInteger)[hits count]; i++) {
        if (NSPointInRect(p, [self rowRect:i])) {
            selection = i;
            [owner launchLookoutResult:nil];
            return;
        }
    }
}

@end

// a small evaluator for the calculator row, digits, + - * / % ^ and brackets only
typedef struct { const char *p; BOOL bad; } LookoutExpr;

static double lookoutSum(LookoutExpr *e);

static void lookoutSkip(LookoutExpr *e)
{
    while (*e->p == ' ')
        e->p++;
}

static double lookoutAtom(LookoutExpr *e)
{
    double v;
    char *end;

    lookoutSkip(e);
    if (*e->p == '(') {
        e->p++;
        v = lookoutSum(e);
        lookoutSkip(e);
        if (*e->p++ != ')')
            e->bad = YES;
        return v;
    }
    if (*e->p == '-') {
        e->p++;
        return -lookoutAtom(e);
    }
    v = strtod(e->p, &end);
    if (end == e->p)
        e->bad = YES;
    e->p = end;
    return v;
}

static double lookoutPower(LookoutExpr *e)
{
    double base = lookoutAtom(e);

    lookoutSkip(e);
    if (*e->p == '^') {
        e->p++;
        return pow(base, lookoutPower(e));
    }
    return base;
}

static double lookoutProduct(LookoutExpr *e)
{
    double v = lookoutPower(e);

    for (;;) {
        char op;

        lookoutSkip(e);
        op = *e->p;
        if (op != '*' && op != '/' && op != '%')
            return v;
        e->p++;
        double r = lookoutPower(e);

        v = op == '*' ? v * r : op == '/' ? v / r : fmod(v, r);
    }
}

static double lookoutSum(LookoutExpr *e)
{
    double v = lookoutProduct(e);

    for (;;) {
        char op;

        lookoutSkip(e);
        op = *e->p;
        if (op != '+' && op != '-')
            return v;
        e->p++;
        v = op == '+' ? v + lookoutProduct(e) : v - lookoutProduct(e);
    }
}

// a sum when the text is only maths and has an operator in it
static NSString *lookoutAnswer(NSString *text)
{
    NSCharacterSet *allowed = [NSCharacterSet characterSetWithCharactersInString:@"0123456789.+-*/%^() "];
    LookoutExpr e;
    double v;

    if ([[text stringByTrimmingCharactersInSet:allowed] length] > 0 ||
        [text rangeOfCharacterFromSet:[NSCharacterSet decimalDigitCharacterSet]].location == NSNotFound ||
        [text rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"+-*/%^"]].location == NSNotFound)
        return nil;
    e.p = [text UTF8String];
    e.bad = NO;
    v = lookoutSum(&e);
    lookoutSkip(&e);
    if (e.bad || *e.p != '\0' || !isfinite(v))
        return nil;
    return [NSString stringWithFormat:@"%.12g", v];
}

// a home folder's icon, Finder's own artwork when it has one for that folder
static NSImage *lookoutFolderIcon(NSString *path)
{
    NSDictionary *special = @{ @"Documents": @"folder-documents", @"Downloads": @"folder-download", @"Desktop": @"user-desktop",
                               @"Music": @"folder-music", @"Pictures": @"folder-pictures", @"Movies": @"folder-videos" };
    NSString *name = special[[path lastPathComponent]] ?: @"folder";
    NSString *file = [NSString stringWithFormat:@"/System/Library/CoreServices/Finder.app/Contents/Resources/%@.png", name];
    NSImage *icon = [[[NSImage alloc] initWithContentsOfFile:file] autorelease];

    return icon ?: [[NSWorkspace sharedWorkspace] iconForFile:path];
}

// 3 for a prefix, 2 for a word that starts with it, 1 for anywhere in the name
static NSInteger lookoutScore(NSString *name, NSString *query)
{
    NSString *n = [name lowercaseString], *q = [query lowercaseString];

    if ([n hasPrefix:q])
        return 3;
    for (NSString *word in [n componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" ._-"]]) {
        if ([word hasPrefix:q])
            return 2;
    }
    return [n rangeOfString:q].location != NSNotFound ? 1 : 0;
}

@implementation MenuBarView

static NSImage *loadImage(NSString *name)
{
    NSString *path = [[NSBundle mainBundle] pathForResource:name ofType:@"png"];

    return path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
}

- (id)initWithFrame:(NSRect)frame
{
    NSMutableArray *status = [NSMutableArray array];
    NSImage *image;

    self = [super initWithFrame:frame];
    _titleRects = [[NSMutableArray alloc] init];
    _beak = loadImage(@"beak");
    // wi-fi is drawn live from PDWiFi, left of these
    for (NSString *name in @[ @"volume", @"battery", @"search" ]) {
        image = loadImage(name);
        if (image != nil) {
            [status addObject:image];
            [image release];
        }
    }
    _status = [status retain];
    _open = NoMenu;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(wifiChanged:) name:PDWiFiChangedNotification object:nil];
    [PDWiFi shared];
    return self;
}

- (void)wifiChanged:(NSNotification *)note
{
    [self setNeedsDisplay:YES];
}

// the left edge of the wi-fi glyph's slot, the first status item left of the image ones
- (CGFloat)wifiX
{
    return NSWidth([self bounds]) - ItemPad - [self widthOfText:[self clockText] bold:NO] - ([_status count] + 1) * (IconSize + 2 * ItemPad);
}

- (void)setDelegate:(id<MenuBarViewDelegate>)delegate
{
    _delegate = delegate;
}

- (void)setMenuData:(NSDictionary *)menuData
{
    [_menuData release];
    _menuData = [menuData retain];
    [self setNeedsDisplay:YES];
}

- (void)tick:(NSTimer *)timer
{
    [self setNeedsDisplay:YES];
}

- (NSFont *)fontBold:(BOOL)bold
{
    NSFont *font = [NSFont menuBarFontOfSize:FontSize];

    if (bold)
        font = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSBoldFontMask];
    return font;
}

- (NSArray *)topItems
{
    return [_menuData objectForKey:@"items"];
}

// the first top-level menu carries the app's name, shown bold as on macOS
- (NSString *)titleAtIndex:(NSUInteger)i
{
    NSDictionary *item = [[self topItems] objectAtIndex:i];
    NSString *title = [item objectForKey:@"title"];

    if (i == 0 && [title length] == 0)
        title = [[item objectForKey:@"submenu"] objectForKey:@"title"];
    return title ? title : @"";
}

- (void)drawText:(NSString *)text bold:(BOOL)bold atX:(CGFloat)x
{
    NSFont *font = [self fontBold:bold];
    NSDictionary *attrs = @{ NSFontAttributeName : font, NSForegroundColorAttributeName : lightDark([NSColor colorWithCalibratedWhite:0.1 alpha:1], lookoutRGB(0xf0, 0xf0, 0xf0, 1)) };
    CGFloat lineHeight = [font ascender] - [font descender];

    [text drawAtPoint:NSMakePoint(x, floor((MenuBarHeight - lineHeight) / 2)) withAttributes:attrs];
}

- (CGFloat)widthOfText:(NSString *)text bold:(BOOL)bold
{
    return [text sizeWithAttributes:@{ NSFontAttributeName : [self fontBold:bold] }].width;
}

- (void)drawHighlight:(NSRect)rect
{
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 0, 3) xRadius:5 yRadius:5];

    [[NSColor colorWithCalibratedWhite:darkNow() ? 1 : 0 alpha:darkNow() ? 0.14 : 0.1] setFill];
    [path fill];
}

// Settings' Date & Time switches, read on every tick so a change shows within the minute
- (NSString *)clockText
{
    static NSDateFormatter *format;
    static NSString *current;
    id h24, date, iso;
    NSString *wanted;

    CFPreferencesAppSynchronize(CFSTR("org.puredarwin.MenuBar"));
    h24 = [(id)CFPreferencesCopyAppValue(CFSTR("Clock24"), CFSTR("org.puredarwin.MenuBar")) autorelease];
    date = [(id)CFPreferencesCopyAppValue(CFSTR("ShowDate"), CFSTR("org.puredarwin.MenuBar")) autorelease];
    iso = [(id)CFPreferencesCopyAppValue(CFSTR("ISO8601"), CFSTR("org.puredarwin.MenuBar")) autorelease];
    wanted = [NSString stringWithFormat:@"%@%@", (date == nil || [date boolValue]) ? ([iso boolValue] ? @"yyyy-MM-dd " : @"EEE MMM d  ") : @"",
                                        ([iso boolValue] || [h24 boolValue]) ? @"HH:mm" : @"h:mm a"];
    if (format == nil)
        format = [[NSDateFormatter alloc] init];
    if (![wanted isEqualToString:current]) {
        [current release];
        current = [wanted copy];
        [format setDateFormat:current];
    }
    return [format stringFromDate:[NSDate date]];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect bounds = [self bounds], beakRect, rect;
    NSString *clock = [self clockText];
    NSUInteger count = [[self topItems] count];
    CGFloat x, w;

    // the demo's rgba(246,240,236,.72), the compositor blurs the desktop under it
    [(darkNow() ? lookoutRGB(40, 38, 40, 0.72) : [NSColor colorWithCalibratedRed:0.965 green:0.941 blue:0.925 alpha:0.72]) setFill];
    NSRectFill(bounds);
    // the panel colour darkened by 8%, solid because NSRectFill copies rather than blends
    [(darkNow() ? lookoutRGB(0x1c, 0x1c, 0x1c, 1) : [NSColor colorWithCalibratedRed:0.888 green:0.869 blue:0.851 alpha:1]) setFill];
    NSRectFill(NSMakeRect(0, 0, NSWidth(bounds), 1));

    [_titleRects removeAllObjects];
    beakRect = NSMakeRect(4, 0, IconSize + 2 * 6, MenuBarHeight);
    if (_open == BeakIndex)
        [self drawHighlight:beakRect];
    drawIcon(_beak, NSMakeRect(NSMinX(beakRect) + 6, (MenuBarHeight - IconSize) / 2, IconSize, IconSize));

    x = NSMaxX(beakRect) + 2;
    for (NSUInteger i = 0; i < count; i++) {
        NSString *title = [self titleAtIndex:i];

        w = [self widthOfText:title bold:i == 0] + 2 * ItemPad;
        rect = NSMakeRect(x, 0, w, MenuBarHeight);
        if (_open == (NSInteger)i)
            [self drawHighlight:rect];
        [self drawText:title bold:i == 0 atX:x + ItemPad];
        [_titleRects addObject:[NSValue valueWithRect:rect]];
        x += w;
    }

    x = NSWidth(bounds) - ItemPad - [self widthOfText:clock bold:NO];
    [self drawText:clock bold:NO atX:x];
    for (NSImage *image in [_status reverseObjectEnumerator]) {
        x -= IconSize + 2 * ItemPad;
        drawIcon(image, NSMakeRect(x + ItemPad, (MenuBarHeight - IconSize) / 2, IconSize, IconSize));
    }
    // the signal of the joined network, faint with the radio off, nothing without the hardware
    PDWiFi *wifi = [PDWiFi shared];
    if (wifi.hardware) {
        x -= IconSize + 2 * ItemPad;
        if ([[WiFiMenu shared] isOpen])
            [self drawHighlight:NSMakeRect(x, 0, IconSize + 2 * ItemPad, MenuBarHeight)];
        PDDrawWiFiGlyph(NSMakeRect(x + ItemPad, (MenuBarHeight - IconSize) / 2, IconSize, IconSize), wifi.current ? [wifi.current bars] : 0,
                        [ink() colorWithAlphaComponent:wifi.powered ? 1 : 0.35], NO);
    }
}

- (void)layoutLookout
{
    NSRect screen = [[NSScreen mainScreen] frame];
    CGFloat h = LookoutBarH + ([_lookoutResults count] > 0 ? 1 + LookoutResH : 0);
    // the bar stays put a little above the middle, the results open under it
    CGFloat top = NSMaxY(screen) - floor(NSHeight(screen) * 0.22);
    LookoutView *view = (LookoutView *)[_lookoutWindow contentView];

    view->hits = _lookoutResults;
    view->selection = _lookoutSelection;
    [_lookoutWindow setFrame:NSMakeRect(floor(NSMidX(screen) - LookoutW / 2), top - h, LookoutW, h) display:YES];
    [_lookoutField setFrame:NSMakeRect(48, (LookoutBarH - 32) / 2, LookoutW - 48 - 56, 32)];
    [_lookoutWindow _setBackgroundBlurInset:0 radius:LookoutRadius];
    // a resize swaps the backing store, draw into the new one now rather than on some later pass
    [view setNeedsDisplay:YES];
    [_lookoutWindow displayIfNeeded];
}

- (void)openLookout:(id)sender
{
    if (_lookoutWindow == nil) {
        LookoutView *view = [[[LookoutView alloc] initWithFrame:NSMakeRect(0, 0, LookoutW, LookoutBarH)] autorelease];

        _lookoutWindow = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, LookoutW, LookoutBarH)
                                                       styleMask:NSBorderlessWindowMask
                                                         backing:NSBackingStoreBuffered defer:NO];
        // a modal panel takes the keyboard while it is up, menu levels never do
        [_lookoutWindow setLevel:NSModalPanelWindowLevel];
        [_lookoutWindow setOpaque:NO];
        [_lookoutWindow setBackgroundColor:[NSColor clearColor]];
        [_lookoutWindow setContentView:view];
        [view setAutoresizesSubviews:NO];
        _lookoutField = [[NSTextField alloc] initWithFrame:NSZeroRect];
        [_lookoutField setBordered:NO];
        [_lookoutField setBezeled:NO];
        [_lookoutField setDrawsBackground:NO];
        [_lookoutField setFont:lookoutFont(@"Inter-Light", 24)];
        [_lookoutField setTextColor:ink()];
        // the cell draws the placeholder where the field editor puts the text
        [[_lookoutField cell] setPlaceholderAttributedString:[[NSAttributedString alloc] initWithString:NSLocalizedString(@"Lookout Search", @"")
            attributes:@{ NSFontAttributeName: lookoutFont(@"Inter-Light", 24), NSForegroundColorAttributeName: lightDark(lookoutRGB(0x9a, 0x9a, 0x9a, 1), lookoutRGB(0x7a, 0x7a, 0x7a, 1)) }]];
        [_lookoutField setTarget:self];
        [_lookoutField setAction:@selector(searchLookout:)];
        [_lookoutField setDelegate:self];
        [view addSubview:_lookoutField];
        view->field = _lookoutField;
        view->owner = self;
        view->glass = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"search-large" ofType:@"png"]];
        _lookoutResults = [[NSMutableArray alloc] init];
    }
    [_lookoutField setStringValue:@""];
    [_lookoutResults removeAllObjects];
    _lookoutSelection = 0;
    [self layoutLookout];
    // the cell sets the field editor up from its colour when it takes focus, so the concrete ink goes in first
    NSColor *text = darkNow() ? lookoutRGB(0xee, 0xee, 0xee, 1) : lookoutRGB(0x1b, 0x1b, 0x1b, 1);
    [_lookoutField setTextColor:text];
    [_lookoutWindow makeKeyAndOrderFront:nil];
    [_lookoutWindow makeFirstResponder:_lookoutField];

    // the cell hands the editor its text colour, the caret colour is the editor's own
    NSText *editor = [_lookoutWindow fieldEditor:YES forObject:_lookoutField];
    if ([editor isKindOfClass:[NSTextView class]])
        [(NSTextView *)editor setInsertionPointColor:text];
}

// every installed app once, Finder from CoreServices with them
- (NSArray *)lookoutApplications
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableArray *apps = [NSMutableArray array];

    for (NSString *root in @[ @"/Applications", @"/System/Applications", @"/System/Library/CoreServices" ]) {
        NSDirectoryEnumerator *enumerator = [fm enumeratorAtPath:root];
        NSString *relative;

        while ((relative = [enumerator nextObject]) != nil) {
            if (![[relative pathExtension] isEqualToString:@"app"])
                continue;
            // an app bundle holds nothing else worth listing
            [enumerator skipDescendants];

            NSString *name = [[relative lastPathComponent] stringByDeletingPathExtension];

            // the rest of CoreServices is the desktop itself, not something to open
            if ([root hasSuffix:@"CoreServices"] && ![name isEqualToString:@"Finder"])
                continue;
            [apps addObject:@{ @"name": name, @"path": [root stringByAppendingPathComponent:relative] }];
        }
    }
    return apps;
}

- (void)searchLookout:(id)sender
{
    NSString *query = [[_lookoutField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSWorkspace *ws = [NSWorkspace sharedWorkspace];
    NSMutableArray *apps = [NSMutableArray array], *actions = [NSMutableArray array], *folders = [NSMutableArray array];
    NSString *answer = lookoutAnswer(query);
    NSComparator byScore = ^NSComparisonResult(NSDictionary *x, NSDictionary *y) {
        NSInteger a = [x[@"score"] integerValue], b = [y[@"score"] integerValue];

        return a > b ? NSOrderedAscending : a < b ? NSOrderedDescending : [x[@"name"] caseInsensitiveCompare:y[@"name"]];
    };
    NSString *terminal = nil, *finder = nil;

    [_lookoutResults removeAllObjects];
    _lookoutSelection = 0;
    if ([query length] == 0) {
        [self layoutLookout];
        return;
    }
    if (answer != nil)
        [_lookoutResults addObject:@{ @"group": NSLocalizedString(@"Calculator", @""), @"name": [NSString stringWithFormat:@"%@ = %@", query, answer],
                                        @"kind": NSLocalizedString(@"Calculator", @""), @"answer": answer, @"action": @"copy",
                                        @"icon": [ws iconForFile:@"/Applications/Calculator.app"] ?: [NSNull null] }];

    for (NSDictionary *app in [self lookoutApplications]) {
        NSInteger score = lookoutScore(app[@"name"], query);

        if ([app[@"name"] isEqualToString:@"Terminal"])
            terminal = app[@"path"];
        if ([app[@"name"] isEqualToString:@"Finder"])
            finder = app[@"path"];
        if (score > 0)
            [apps addObject:@{ @"name": app[@"name"], @"path": app[@"path"], @"kind": NSLocalizedString(@"Application", @""), @"action": @"open",
                               @"score": @(score), @"icon": [ws iconForFile:app[@"path"]] }];
    }
    for (NSArray *verb in @[ @[ NSLocalizedString(@"About PureDarwin", @""), @"about", NSLocalizedString(@"System", @"") ], @[ NSLocalizedString(@"New Terminal Window", @""), @"open", NSLocalizedString(@"Action", @""), terminal ?: @"" ],
                             @[ NSLocalizedString(@"New Finder Window", @""), @"open", NSLocalizedString(@"Action", @""), finder ?: @"" ] ]) {
        NSInteger score = lookoutScore(verb[0], query);
        NSString *target = [verb count] > 3 ? verb[3] : nil;

        if (score > 0 && (target == nil || [target length] > 0))
            [actions addObject:@{ @"name": verb[0], @"action": verb[1], @"kind": verb[2], @"score": @(score), @"target": target ?: @"",
                                  @"icon": [ws iconForFile:[target length] ? target : @"/System/Library/CoreServices/Finder.app"] }];
    }
    // the folders a person keeps things in, one level under home
    for (NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:NSHomeDirectory() error:NULL]) {
        NSString *path = [NSHomeDirectory() stringByAppendingPathComponent:name];
        BOOL dir = NO;
        NSInteger score = lookoutScore(name, query);

        if ([name hasPrefix:@"."] || score == 0 || ![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir] || !dir)
            continue;
        [folders addObject:@{ @"name": name, @"path": path, @"kind": NSLocalizedString(@"Folder", @""), @"action": @"open", @"score": @(score),
                              @"icon": lookoutFolderIcon(path) }];
    }
    [apps sortUsingComparator:byScore];
    [actions sortUsingComparator:byScore];
    [folders sortUsingComparator:byScore];

    // the best of everything leads as the top hit, then each group without it
    NSMutableArray *all = [NSMutableArray arrayWithArray:apps];

    [all addObjectsFromArray:actions];
    [all addObjectsFromArray:folders];
    [all sortUsingComparator:byScore];
    if ([all count] > 0) {
        NSMutableDictionary *top = [[all[0] mutableCopy] autorelease];

        top[@"group"] = NSLocalizedString(@"Top Hit", @"");
        [_lookoutResults addObject:top];
    }
    for (NSArray *group in @[ @[ NSLocalizedString(@"Applications", @""), apps ], @[ NSLocalizedString(@"Actions", @""), actions ], @[ NSLocalizedString(@"Folders", @""), folders ] ]) {
        for (NSDictionary *hit in group[1]) {
            NSMutableDictionary *row;

            if ([all count] > 0 && hit == all[0])
                continue;
            row = [[hit mutableCopy] autorelease];
            row[@"group"] = group[0];
            [_lookoutResults addObject:row];
        }
    }
    // a missing icon is left out of the row
    for (NSMutableDictionary *hit in _lookoutResults) {
        if ([hit isKindOfClass:[NSMutableDictionary class]] && hit[@"icon"] == [NSNull null])
            [hit removeObjectForKey:@"icon"];
    }
    [self layoutLookout];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    if ([note object] == _lookoutField)
        [self searchLookout:nil];
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)selector
{
    NSInteger count = (NSInteger)[_lookoutResults count];

    if (control != _lookoutField)
        return NO;
    if (selector == @selector(moveDown:) || selector == @selector(moveUp:)) {
        _lookoutSelection = MAX(0, MIN(count - 1, _lookoutSelection + (selector == @selector(moveDown:) ? 1 : -1)));
        [self layoutLookout];
        return YES;
    }
    if (selector == @selector(insertNewline:)) {
        [self launchLookoutResult:nil];
        return YES;
    }
    // Esc clears the search first and closes on the second press
    if (selector == @selector(cancelOperation:)) {
        // mid-edit the text lives in the field editor, the field's own value can trail it
        if ([[textView string] length] > 0) {
            // edited like a person would, so the editor's selection stays valid (setString: mid-command crashed it)
            [textView selectAll:nil];
            [textView deleteBackward:nil];
            [self searchLookout:nil];
        } else {
            // the editor is still inside this command, so it ends editing on the next pass
            [self performSelector:@selector(closeLookout) withObject:nil afterDelay:0];
        }
        return YES;
    }
    return NO;
}

// editing ends before the panel goes, a hidden field editor left first responder took no keys on the next open
- (void)closeLookout
{
    [_lookoutWindow makeFirstResponder:_lookoutWindow];
    [_lookoutWindow orderOut:nil];
}

- (void)launchLookoutResult:(id)sender
{
    NSInteger index = ((LookoutView *)[_lookoutWindow contentView])->selection;
    NSDictionary *hit;

    if (index < 0 || index >= (NSInteger)[_lookoutResults count])
        return;
    hit = _lookoutResults[index];
    [self closeLookout];
    if ([hit[@"action"] isEqualToString:@"copy"]) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];

        [pb declareTypes:@[ NSStringPboardType ] owner:nil];
        [pb setString:hit[@"answer"] forType:NSStringPboardType];
    } else if ([hit[@"action"] isEqualToString:@"about"]) {
        [[AboutPanel sharedPanel] show];
    } else if ([hit[@"kind"] isEqualToString:@"Folder"]) {
        [[NSWorkspace sharedWorkspace] openFile:hit[@"path"]];
    } else {
        [[NSWorkspace sharedWorkspace] launchApplication:[hit[@"target"] length] ? hit[@"target"] : hit[@"path"]];
    }
}

// rebuilds a published menu as a real NSMenu whose items report their index path
- (NSMenu *)menuFromData:(NSDictionary *)data path:(NSArray *)path
{
    NSMenu *menu = [[[NSMenu alloc] initWithTitle:[data objectForKey:@"title"] ?: @""] autorelease];
    NSArray *items = [data objectForKey:@"items"];

    [menu setAutoenablesItems:NO];
    for (NSUInteger i = 0; i < [items count]; i++) {
        NSDictionary *d = [items objectAtIndex:i];
        NSArray *here = [path arrayByAddingObject:[NSNumber numberWithUnsignedInteger:i]];
        NSMenuItem *item;

        if ([[d objectForKey:@"isSeparator"] boolValue]) {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        item = [[NSMenuItem alloc] initWithTitle:[d objectForKey:@"title"] ?: @""
                                          action:@selector(pick:)
                                   keyEquivalent:[d objectForKey:@"keyEquivalent"] ?: @""];
        [item setKeyEquivalentModifierMask:[[d objectForKey:@"keyEquivalentModifierMask"] unsignedIntegerValue]];
        [item setTarget:self];
        [item setRepresentedObject:here];
        [item setEnabled:[[d objectForKey:@"enabled"] boolValue]];
        [item setState:[[d objectForKey:@"state"] integerValue]];
        if ([d objectForKey:@"submenu"] != nil)
            [item setSubmenu:[self menuFromData:[d objectForKey:@"submenu"] path:here]];
        [menu addItem:item];
        [item release];
    }
    return menu;
}

// the system menu from the demo: about, settings, a terminal, then power
- (NSMenu *)beakMenu
{
    NSMenu *menu = [[[NSMenu alloc] initWithTitle:@"PureDarwin"] autorelease];
    NSMenuItem *item;

    [menu setAutoenablesItems:NO];
    item = [menu addItemWithTitle:NSLocalizedString(@"About PureDarwin", @"") action:@selector(openAbout:) keyEquivalent:@""];
    [item setTarget:self];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:NSLocalizedString(@"Settings\u2026", @"") action:@selector(openSettings:) keyEquivalent:@""];
    [item setTarget:self];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:@"Terminal" action:@selector(openTerminal:) keyEquivalent:@""];
    [item setTarget:self];
    [menu addItem:[NSMenuItem separatorItem]];
    for (NSString *title in @[ NSLocalizedString(@"Sleep", @""), NSLocalizedString(@"Restart\u2026", @""), NSLocalizedString(@"Shut Down\u2026", @"") ])
        [[menu addItemWithTitle:title action:NULL keyEquivalent:@""] setEnabled:NO];
    return menu;
}

- (void)openAbout:(id)sender
{
    [[AboutPanel sharedPanel] show];
}

- (void)openSettings:(id)sender
{
    [[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/Settings.app"];
}

- (void)openTerminal:(id)sender
{
    [[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/Utilities/Terminal.app"];
}

- (void)pick:(NSMenuItem *)item
{
    [_delegate menuBarView:self pickedPath:[item representedObject]];
}

// the menu for the beak (-1) or an app title, and where it drops down from
- (NSMenu *)menuAtIndex:(NSInteger)i left:(CGFloat *)left
{
    NSDictionary *sub;
    NSRect rect;

    if (i == BeakIndex) {
        *left = 4;
        return [self beakMenu];
    }
    rect = [[_titleRects objectAtIndex:i] rectValue];
    sub = [[[self topItems] objectAtIndex:i] objectForKey:@"submenu"];
    *left = NSMinX(rect);
    return sub ? [self menuFromData:sub path:@[ [NSNumber numberWithUnsignedInteger:i] ]] : nil;
}

// the other titles, in screen coordinates, so sliding onto one switches menus as on macOS
- (NSArray *)exitRectsExcept:(NSInteger)open indexes:(NSMutableArray *)indexes
{
    NSMutableArray *rects = [NSMutableArray array];
    NSRect beak = NSMakeRect(4, 0, IconSize + 2 * 6, MenuBarHeight);

    for (NSInteger i = BeakIndex; i < (NSInteger)[_titleRects count]; i++) {
        NSRect r = (i == BeakIndex) ? beak : [[_titleRects objectAtIndex:i] rectValue];

        if (i == open)
            continue;
        // the same conversion the menu tracking applies to events, so both agree on screen coordinates
        r = [self convertRect:r toView:nil];
        r.origin = [[self window] convertBaseToScreen:r.origin];
        [rects addObject:[NSValue valueWithRect:r]];
        [indexes addObject:[NSNumber numberWithInteger:i]];
    }
    return rects;
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger open = NoMenu;
    NSMenu *menu;
    CGFloat left;

    // Lookout is the first status icon to the left of the clock.
    CGFloat searchX = NSWidth([self bounds]) - ItemPad - [self widthOfText:[self clockText] bold:NO]
                    - (IconSize + 2 * ItemPad);
    if (p.x >= searchX && p.x < searchX + IconSize + 2 * ItemPad) {
        [self openLookout:nil];
        return;
    }
    if ([PDWiFi shared].hardware && p.x >= [self wifiX] && p.x < [self wifiX] + IconSize + 2 * ItemPad) {
        NSPoint screen = [[self window] convertBaseToScreen:[self convertPoint:NSMakePoint([self wifiX], 0) toView:nil]];
        [[WiFiMenu shared] toggleAtX:screen.x];
        [self setNeedsDisplay:YES];
        return;
    }

    if (p.x < IconSize + 2 * 6 + 4) {
        open = BeakIndex;
    } else {
        for (NSUInteger i = 0; i < [_titleRects count]; i++) {
            if (NSPointInRect(p, [[_titleRects objectAtIndex:i] rectValue])) {
                open = i;
                break;
            }
        }
    }

    // each pass shows one menu, sliding onto another title ends it and the loop opens that one
    while (open != NoMenu && (menu = [self menuAtIndex:open left:&left]) != nil) {
        NSMutableArray *indexes = [NSMutableArray array];
        NSInteger exit;

        _open = open;
        // the highlight is drawn before the menu takes over the run loop
        [self display];
        [NSMenu _pdSetTrackingExitRects:[self exitRectsExcept:open indexes:indexes]];
        [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(left, 0) inView:self];
        exit = [NSMenu _pdTrackingExitIndex];
        open = exit >= 0 ? [[indexes objectAtIndex:exit] integerValue] : NoMenu;
    }
    _open = NoMenu;
    [self setNeedsDisplay:YES];
}

@end
