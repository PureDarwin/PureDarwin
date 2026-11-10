// PDUtility.m - what the utility apps share: palette, unified toolbar, sidebars, status lines, glyphs and sheets
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import "PDUtility.h"

BOOL PDDark(void)
{
    return [[NSAppearance currentDrawingAppearance] _isDark];
}

NSColor *PDFixed(unsigned rgb)
{
    return [NSColor colorWithCalibratedRed:(rgb >> 16) / 255.0 green:((rgb >> 8) & 0xff) / 255.0 blue:(rgb & 0xff) / 255.0 alpha:1];
}

NSColor *PDColor(unsigned light, unsigned dark)
{
    return [NSColor colorWithName:nil dynamicProvider:^NSColor *(NSAppearance *a) {
        return PDFixed([a _isDark] ? dark : light);
    }];
}

NSColor *PDAlpha(NSColor *color, CGFloat alpha)
{
    return [color colorWithAlphaComponent:alpha];
}

// a weight the system lacks falls back to Inter Regular, the system font can be missing too
NSFont *PDFont(NSString *name, CGFloat size)
{
    NSFont *f = [NSFont fontWithName:name size:size];

    if (f == nil)
        f = [NSFont fontWithName:@"Inter-Regular" size:size];
    return f != nil ? f : [NSFont systemFontOfSize:size];
}

// the demo's monospace stack, whichever face the system has
NSFont *PDMono(CGFloat size)
{
    for (NSString *name in @[ @"SourceCodePro-Regular", @"JetBrainsMono-Regular", @"DejaVuSansMono", @"Menlo-Regular" ]) {
        NSFont *f = [NSFont fontWithName:name size:size];

        if (f != nil)
            return f;
    }
    return [NSFont userFixedPitchFontOfSize:size] ?: PDFont(@"Inter-Regular", size);
}

NSString *PDFitted(NSString *text, NSDictionary *attrs, CGFloat width)
{
    NSUInteger n = [text length];

    if ([text sizeWithAttributes:attrs].width <= width)
        return text;
    while (n > 1) {
        NSString *cut = [[text substringToIndex:--n] stringByAppendingString:@"…"];

        if ([cut sizeWithAttributes:attrs].width <= width)
            return cut;
    }
    return @"…";
}

// decimal units like the demo, 1.42 MB and 61.5 GB
NSString *PDBytes(double n)
{
    NSArray *units = @[ @"B", @"KB", @"MB", @"GB", @"TB" ];
    NSUInteger i = 0;

    while (n >= 1000 && i < [units count] - 1) {
        n /= 1000;
        i++;
    }
    if (i == 0)
        return [NSString stringWithFormat:@"%.0f B", n];
    return [NSString stringWithFormat:n < 10 ? @"%.2f %@" : n < 100 ? @"%.1f %@" : @"%.0f %@", n, units[i]];
}

NSDictionary *PDText(NSFont *font, NSColor *color)
{
    return @{ NSFontAttributeName: font, NSForegroundColorAttributeName: color };
}

NSImage *PDResourceImage(NSString *name)
{
    static NSMutableDictionary *cache;
    NSImage *image;

    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    image = [cache objectForKey:name];
    if (image == nil) {
        NSString *png = [[NSBundle mainBundle] pathForResource:name ofType:@"png"];

        image = png != nil ? [[[NSImage alloc] initWithContentsOfFile:png] autorelease] : nil;
        if (image == nil)
            return nil;
        [cache setObject:image forKey:name];
    }
    return image;
}

void PDFillRounded(NSRect r, CGFloat radius, NSColor *color)
{
    [color setFill];
    [[NSBezierPath bezierPathWithRoundedRect:r xRadius:radius yRadius:radius] fill];
}

// glyphs on a 16 point grid, stroked like the demo's toolbar icons, the view is flipped
void PDDrawGlyph(NSString *name, NSRect r, NSColor *color)
{
    NSBezierPath *p = [NSBezierPath bezierPath];
    CGFloat s = NSWidth(r) / 16, x0 = NSMinX(r), y0 = NSMinY(r);
    NSPoint (^P)(CGFloat, CGFloat) = ^NSPoint(CGFloat x, CGFloat y) { return NSMakePoint(x0 + x * s, y0 + y * s); };
    NSRect (^R)(CGFloat, CGFloat, CGFloat, CGFloat) = ^NSRect(CGFloat x, CGFloat y, CGFloat w, CGFloat h) {
        return NSMakeRect(x0 + x * s, y0 + y * s, w * s, h * s);
    };
    BOOL fill = NO;

    if ([name isEqualToString:@"back"] || [name isEqualToString:@"forward"]) {
        CGFloat d = [name isEqualToString:@"back"] ? 1 : -1;
        [p moveToPoint:P(8 + 2 * d, 3)];
        [p lineToPoint:P(8 - 3 * d, 8)];
        [p lineToPoint:P(8 + 2 * d, 13)];
    } else if ([name isEqualToString:@"stop"] || [name isEqualToString:@"info"]) {
        [p appendBezierPathWithOvalInRect:R(2, 2, 12, 12)];
        if ([name isEqualToString:@"stop"]) {
            [p moveToPoint:P(5.8, 5.8)];
            [p lineToPoint:P(10.2, 10.2)];
            [p moveToPoint:P(10.2, 5.8)];
            [p lineToPoint:P(5.8, 10.2)];
        } else {
            [p moveToPoint:P(8, 7.2)];
            [p lineToPoint:P(8, 11.2)];
            [p moveToPoint:P(8, 4.9)];
            [p lineToPoint:P(8, 5.1)];
        }
    } else if ([name isEqualToString:@"pause"]) {
        [p moveToPoint:P(5.5, 3.5)];
        [p lineToPoint:P(5.5, 12.5)];
        [p moveToPoint:P(10.5, 3.5)];
        [p lineToPoint:P(10.5, 12.5)];
    } else if ([name isEqualToString:@"play"]) {
        [p moveToPoint:P(5, 3.5)];
        [p lineToPoint:P(12, 8)];
        [p lineToPoint:P(5, 12.5)];
        [p closePath];
    } else if ([name isEqualToString:@"clear"]) {
        [p moveToPoint:P(3, 4.5)];
        [p lineToPoint:P(13, 4.5)];
        [p moveToPoint:P(6.5, 4.5)];
        [p lineToPoint:P(6.5, 3)];
        [p lineToPoint:P(9.5, 3)];
        [p lineToPoint:P(9.5, 4.5)];
        [p moveToPoint:P(4.5, 4.5)];
        [p lineToPoint:P(5.2, 13)];
        [p lineToPoint:P(10.8, 13)];
        [p lineToPoint:P(11.5, 4.5)];
    } else if ([name isEqualToString:@"zoomin"] || [name isEqualToString:@"zoomout"] || [name isEqualToString:@"search"]) {
        [p appendBezierPathWithOvalInRect:R(2.5, 2.5, 9, 9)];
        [p moveToPoint:P(10.4, 10.4)];
        [p lineToPoint:P(14, 14)];
        if (![name isEqualToString:@"search"]) {
            [p moveToPoint:P(5, 7)];
            [p lineToPoint:P(9, 7)];
        }
        if ([name isEqualToString:@"zoomin"]) {
            [p moveToPoint:P(7, 5)];
            [p lineToPoint:P(7, 9)];
        }
    } else if ([name isEqualToString:@"fit"]) {
        for (int i = 0; i < 4; i++) {
            CGFloat cx = i & 1 ? 13.5 : 2.5, cy = i & 2 ? 13.5 : 2.5, dx = i & 1 ? -3.5 : 3.5, dy = i & 2 ? -3.5 : 3.5;
            [p moveToPoint:P(cx, cy + dy)];
            [p lineToPoint:P(cx, cy)];
            [p lineToPoint:P(cx + dx, cy)];
        }
    } else if ([name isEqualToString:@"rotl"] || [name isEqualToString:@"rotr"]) {
        BOOL left = [name isEqualToString:@"rotl"];
        [p appendBezierPathWithArcWithCenter:P(8, 8) radius:5 * s startAngle:left ? 200 : -20 endAngle:left ? -60 : 240 clockwise:!left];
        [p moveToPoint:P(left ? 3 : 13, 2.5)];
        [p lineToPoint:P(left ? 3 : 13, 5.5)];
        [p lineToPoint:P(left ? 6 : 10, 5.5)];
    } else if ([name isEqualToString:@"check"]) {
        [p moveToPoint:P(8, 13.5)];
        [p curveToPoint:P(2.5, 6.5) controlPoint1:P(8, 13.5) controlPoint2:P(2.5, 10.3)];
        [p curveToPoint:P(8, 4.6) controlPoint1:P(2.5, 3.5) controlPoint2:P(6.5, 2.5)];
        [p curveToPoint:P(13.5, 6.5) controlPoint1:P(9.5, 2.5) controlPoint2:P(13.5, 3.5)];
        [p curveToPoint:P(8, 13.5) controlPoint1:P(13.5, 10.3) controlPoint2:P(8, 13.5)];
        [p moveToPoint:P(8, 6.5)];
        [p lineToPoint:P(8, 10.5)];
        [p moveToPoint:P(6, 8.5)];
        [p lineToPoint:P(10, 8.5)];
    } else if ([name isEqualToString:@"partition"]) {
        [p appendBezierPathWithOvalInRect:R(2, 2, 12, 12)];
        [p moveToPoint:P(8, 2)];
        [p lineToPoint:P(8, 8)];
        [p lineToPoint:P(13, 11)];
    } else if ([name isEqualToString:@"erase"]) {
        [p moveToPoint:P(6.5, 13.5)];
        [p lineToPoint:P(13.5, 13.5)];
        [p moveToPoint:P(3, 10)];
        [p lineToPoint:P(9, 3.5)];
        [p lineToPoint:P(13, 7.5)];
        [p lineToPoint:P(7.5, 13.5)];
        [p lineToPoint:P(5.5, 13.5)];
        [p closePath];
    } else if ([name isEqualToString:@"eject"]) {
        [p moveToPoint:P(8, 3)];
        [p lineToPoint:P(13, 8.5)];
        [p lineToPoint:P(3, 8.5)];
        [p closePath];
        [p moveToPoint:P(3, 12.5)];
        [p lineToPoint:P(13, 12.5)];
    } else if ([name isEqualToString:@"mount"]) {
        [p moveToPoint:P(8, 13)];
        [p lineToPoint:P(8, 4)];
        [p moveToPoint:P(4.5, 7.5)];
        [p lineToPoint:P(8, 4)];
        [p lineToPoint:P(11.5, 7.5)];
        [p moveToPoint:P(3, 13.5)];
        [p lineToPoint:P(13, 13.5)];
    } else if ([name isEqualToString:@"plus"]) {
        [p moveToPoint:P(8, 3)];
        [p lineToPoint:P(8, 13)];
        [p moveToPoint:P(3, 8)];
        [p lineToPoint:P(13, 8)];
    } else if ([name isEqualToString:@"screen"]) {
        [p appendBezierPathWithRoundedRect:R(1.5, 3, 13, 9.5) xRadius:s yRadius:s];
    } else if ([name isEqualToString:@"window"]) {
        [p appendBezierPathWithRoundedRect:R(1.5, 3, 13, 9.5) xRadius:s yRadius:s];
        [p appendBezierPathWithRoundedRect:R(4.5, 5.5, 7, 4.5) xRadius:s * .6 yRadius:s * .6];
    } else if ([name isEqualToString:@"selection"]) {
        float dash[2] = { 2 * s, 1.6 * s };
        [p appendBezierPathWithRoundedRect:R(2.5, 4, 11, 8) xRadius:s * .6 yRadius:s * .6];
        [p setLineDash:dash count:2 phase:0];
    }
    [color set];
    [p setLineWidth:MAX(1, 1.5 * s)];
    [p setLineCapStyle:NSRoundLineCapStyle];
    [p setLineJoinStyle:NSRoundLineJoinStyle];
    if (fill)
        [p fill];
    [p stroke];
}

void PDDrawToolbar(NSView *view, NSRect bar)
{
    BOOL key = [[view window] isKeyWindow];

    // the demo's gradient from #f9f9f9 to #f0f0f0 a row at a time, flat while inactive
    for (int row = 0; row < NSHeight(bar); row++) {
        CGFloat t = row / (NSHeight(bar) - 1);
        CGFloat v = PDDark() ? (key ? 0x33 - 7 * t : 0x2c) : (key ? 0xf9 - 9 * t : 0xf6);

        [[NSColor colorWithCalibratedWhite:v / 255.0 alpha:1] setFill];
        NSRectFill(NSMakeRect(NSMinX(bar), NSMinY(bar) + row, NSWidth(bar), 1));
    }
    [PDColor(0xdcdcdc, 0x141414) setFill];
    NSRectFill(NSMakeRect(NSMinX(bar), NSMaxY(bar), NSWidth(bar), 1));
}

void PDDrawSidebar(NSRect side)
{
    [PDColor(0xe8e6e6, 0x302e30) setFill];
    NSRectFill(side);
    [PDColor(0xd6d6d6, 0x141414) setFill];
    NSRectFill(NSMakeRect(NSMaxX(side), NSMinY(side), 1, NSHeight(side)));
}

void PDDrawStatus(NSRect r, NSString *left, NSString *right)
{
    NSDictionary *attrs = PDText(PDFont(@"Inter-Regular", 11), PDColor(0x777777, 0x8a8a8a));
    NSSize ls = [left sizeWithAttributes:attrs], rs = [right sizeWithAttributes:attrs];

    [PDColor(0xfafafa, 0x262628) setFill];
    NSRectFill(r);
    [PDColor(0xe2e2e2, 0x141414) setFill];
    NSRectFill(NSMakeRect(NSMinX(r), NSMinY(r), NSWidth(r), 1));
    if (left != nil)
        [PDFitted(left, attrs, NSWidth(r) - rs.width - 40) drawAtPoint:NSMakePoint(NSMinX(r) + 12, NSMinY(r) + floor((NSHeight(r) - ls.height) / 2)) withAttributes:attrs];
    if (right != nil)
        [right drawAtPoint:NSMakePoint(NSMaxX(r) - 12 - rs.width, NSMinY(r) + floor((NSHeight(r) - rs.height) / 2)) withAttributes:attrs];
}

void PDDrawTitle(NSString *title, NSString *subtitle, NSPoint at, CGFloat maxWidth, BOOL onDark)
{
    NSDictionary *bold = PDText(PDFont(@"Inter-Bold", 13), onDark ? PDFixed(0xe6e6e6) : PDColor(0x1b1b1b, 0xe6e6e6));
    NSDictionary *thin = PDText(PDFont(@"Inter-Regular", 11.5), PDColor(0x8a8a8a, 0x8a8a8a));
    NSSize ts = [title sizeWithAttributes:bold];

    title = PDFitted(title, bold, maxWidth);
    [title drawAtPoint:NSMakePoint(at.x, at.y - ts.height / 2) withAttributes:bold];
    if (subtitle != nil && ts.width + 12 < maxWidth) {
        NSSize ss = [subtitle sizeWithAttributes:thin];
        [PDFitted(subtitle, thin, maxWidth - ts.width - 8) drawAtPoint:NSMakePoint(at.x + MIN(ts.width, maxWidth) + 6, at.y - ss.height / 2 + 1) withAttributes:thin];
    }
}

void PDDrawSectionHeading(NSString *text, NSPoint at)
{
    NSMutableDictionary *attrs = [[PDText(PDFont(@"Inter-SemiBold", 11), PDColor(0x8a8a8a, 0x8a8a8a)) mutableCopy] autorelease];

    [text drawAtPoint:at withAttributes:attrs];
}

@implementation PDToolButton

+ (instancetype)buttonWithGlyph:(NSString *)glyph label:(NSString *)label target:(id)target action:(SEL)action
{
    PDToolButton *b = [[[PDToolButton alloc] init] autorelease];

    b->glyph = [glyph copy];
    b->label = [label copy];
    b->target = target;
    b->action = action;
    b->enabled = YES;
    return b;
}

- (void)dealloc
{
    [glyph release];
    [label release];
    [super dealloc];
}

- (CGFloat)width
{
    CGFloat w = glyph != nil ? 16 : 0;

    if (label != nil)
        w += (glyph != nil ? 5 : 0) + ceil([label sizeWithAttributes:PDText(PDFont(@"Inter-SemiBold", 12), [NSColor blackColor])].width);
    return w + 14;
}

- (void)drawAt:(NSPoint)origin height:(CGFloat)h onDark:(BOOL)onDark
{
    NSColor *ink = !enabled ? (onDark ? PDFixed(0x5a5a5c) : PDColor(0xc4c4c4, 0x555555))
                            : on ? (onDark ? PDFixed(0xffffff) : PDColor(0x1b1b1b, 0xffffff)) : (onDark ? PDFixed(0xd0d0d0) : PDColor(0x4a4a4a, 0xc4c4c4));
    CGFloat x = origin.x + 7;

    frame = NSMakeRect(origin.x, origin.y, [self width], h);
    if (hidden)
        return;
    if (on)
        PDFillRounded(frame, 6, onDark ? PDAlpha(PDFixed(0xffffff), .14) : PDAlpha(PDColor(0x000000, 0xffffff), .1));
    if (glyph != nil) {
        PDDrawGlyph(glyph, NSMakeRect(x, NSMidY(frame) - 8, 16, 16), ink);
        x += 21;
    }
    if (label != nil) {
        NSDictionary *attrs = PDText(PDFont(@"Inter-SemiBold", 12), ink);
        NSSize sz = [label sizeWithAttributes:attrs];
        [label drawAtPoint:NSMakePoint(x, floor(NSMidY(frame) - sz.height / 2)) withAttributes:attrs];
    }
}

@end

CGFloat PDLayoutButtons(NSArray *buttons, CGFloat right, CGFloat barH, BOOL onDark)
{
    CGFloat x = right;

    for (PDToolButton *b in [buttons reverseObjectEnumerator]) {
        if (b->hidden)
            continue;
        x -= [b width];
        [b drawAt:NSMakePoint(x, floor((barH - 26) / 2)) height:26 onDark:onDark];
        x -= 2;
    }
    return x;
}

PDToolButton *PDButtonAt(NSArray *buttons, NSPoint p)
{
    for (PDToolButton *b in buttons) {
        if (!b->hidden && NSPointInRect(p, b->frame))
            return b;
    }
    return nil;
}

BOOL PDClickButtons(NSArray *buttons, NSPoint p)
{
    PDToolButton *b = PDButtonAt(buttons, p);

    if (b == nil)
        return NO;
    if (b->enabled && b->action != NULL)
        [NSApp sendAction:b->action to:b->target from:b];
    return YES;
}

@implementation PDSegments

- (void)dealloc
{
    [labels release];
    [super dealloc];
}

- (void)drawCenteredIn:(NSRect)r
{
    NSDictionary *attrs = PDText(PDFont(@"Inter-SemiBold", 12), PDColor(0x1b1b1b, 0xeeeeee));
    CGFloat w = 4;
    NSMutableArray *widths = [NSMutableArray array];

    for (NSString *l in labels) {
        CGFloat lw = ceil([l sizeWithAttributes:attrs].width) + 22;
        [widths addObject:@(lw)];
        w += lw;
    }
    frame = NSMakeRect(floor(NSMidX(r) - w / 2), floor(NSMidY(r) - 13), w, 26);
    PDFillRounded(frame, 7, PDAlpha(PDColor(0x000000, 0xffffff), .07));
    CGFloat x = NSMinX(frame) + 2;
    for (NSUInteger i = 0; i < [labels count]; i++) {
        CGFloat lw = [widths[i] doubleValue];
        NSRect seg = NSMakeRect(x, NSMinY(frame) + 2, lw, 22);
        NSSize sz = [labels[i] sizeWithAttributes:attrs];

        if ((NSInteger)i == selected)
            PDFillRounded(seg, 5, PDColor(0xffffff, 0x575153));
        [labels[i] drawAtPoint:NSMakePoint(floor(NSMidX(seg) - sz.width / 2), floor(NSMidY(seg) - sz.height / 2)) withAttributes:attrs];
        x += lw;
    }
}

- (NSInteger)segmentAt:(NSPoint)p
{
    if (!NSPointInRect(p, frame))
        return -1;
    return MIN((NSInteger)[labels count] - 1, (NSInteger)((p.x - NSMinX(frame)) / (NSWidth(frame) / [labels count])));
}

@end

@implementation PDSheet

+ (PDSheet *)showInView:(NSView *)host icon:(NSString *)icon title:(NSString *)title message:(NSString *)message
                  field:(NSString *)field mono:(NSString *)mono buttons:(NSArray *)buttons styles:(NSArray *)styles
                   done:(void (^)(NSInteger index, NSString *field))done
{
    PDSheet *s = [[[PDSheet alloc] initWithFrame:[host bounds]] autorelease];

    s->_icon = [icon copy];
    s->_title = [title copy];
    s->_message = [message copy];
    s->_mono = [mono copy];
    s->_buttons = [buttons copy];
    s->_styles = [styles copy];
    s->_done = [done copy];
    s->_rects = [[NSMutableArray alloc] init];
    [s setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    if (field != nil) {
        s->_field = [[NSTextField alloc] initWithFrame:NSZeroRect];
        [s->_field setStringValue:field];
        [s->_field setFont:PDFont(@"Inter-Regular", 12.5)];
        [s->_field setTarget:s];
        [s->_field setAction:@selector(fieldDone:)];
        [s addSubview:s->_field];
    }
    [host addSubview:s];
    [s placeField];
    if (s->_field != nil) {
        [[host window] makeFirstResponder:s->_field];
        [[s->_field currentEditor] selectAll:nil];
    }
    return s;
}

- (void)dealloc
{
    [_icon release];
    [_title release];
    [_message release];
    [_mono release];
    [_buttons release];
    [_styles release];
    [_field release];
    [_rects release];
    [_done release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSRect)panelRect
{
    NSRect b = [self bounds];
    CGFloat w = MIN(440, NSWidth(b) - 24), h = 132 + (_field != nil ? 34 : 0) + (_mono != nil ? 132 : 0);
    NSDictionary *msg = PDText(PDFont(@"Inter-Regular", 12), [NSColor blackColor]);
    NSRect text = [_message boundingRectWithSize:NSMakeSize(w - 102, 1000) options:NSStringDrawingUsesLineFragmentOrigin attributes:msg];

    h += MAX(0, NSHeight(text) - 16);
    return NSMakeRect(floor(NSMidX(b) - w / 2), PD_TOOLBAR_H, w, h);
}

- (void)placeField
{
    NSRect p = [self panelRect];

    if (_field != nil)
        [_field setFrame:NSMakeRect(NSMinX(p) + 82, NSMaxY(p) - 52 - 34 - (_mono != nil ? 132 : 0), NSWidth(p) - 102, 24)];
}

- (void)resizeWithOldSuperviewSize:(NSSize)old
{
    [super resizeWithOldSuperviewSize:old];
    [self placeField];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect p = [self panelRect];
    NSDictionary *head = PDText(PDFont(@"Inter-SemiBold", 13), PDColor(0x1b1b1b, 0xeeeeee));
    NSDictionary *msg = PDText(PDFont(@"Inter-Regular", 12), PDColor(0x6e6e6e, 0xa0a0a0));
    NSImage *icon = PDResourceImage(_icon);
    CGFloat x, y = NSMinY(p) + 20;

    [PDAlpha([NSColor blackColor], .08) setFill];
    NSRectFillUsingOperation(NSMakeRect(0, PD_TOOLBAR_H, NSWidth([self bounds]), NSHeight([self bounds])), NSCompositeSourceOver);
    [NSGraphicsContext saveGraphicsState];
    NSShadow *shadow = [[[NSShadow alloc] init] autorelease];
    [shadow setShadowBlurRadius:20];
    [shadow setShadowOffset:NSMakeSize(0, -8)];
    [shadow setShadowColor:PDAlpha([NSColor blackColor], .3)];
    [shadow set];
    PDFillRounded(p, 12, PDColor(0xf6f3f1, 0x2c2a2c));
    [NSGraphicsContext restoreGraphicsState];

    [icon drawInRect:NSMakeRect(NSMinX(p) + 20, y, 48, 48) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    [_title drawAtPoint:NSMakePoint(NSMinX(p) + 82, y) withAttributes:head];
    [_message drawInRect:NSMakeRect(NSMinX(p) + 82, y + 22, NSWidth(p) - 102, 200) withAttributes:msg];

    if (_mono != nil) {
        NSRect box = NSMakeRect(NSMinX(p) + 82, NSMaxY(p) - 52 - 124, NSWidth(p) - 102, 116);
        PDFillRounded(box, 6, PDFixed(0x1f2023));
        [_mono drawInRect:NSInsetRect(box, 9, 7) withAttributes:PDText(PDMono(11), PDFixed(0xd8d8d8))];
    }

    [_rects removeAllObjects];
    x = NSMaxX(p) - 20;
    for (NSInteger i = [_buttons count] - 1; i >= 0; i--) {
        NSDictionary *attrs = PDText(PDFont(@"Inter-Regular", 12.5), [NSColor blackColor]);
        NSString *style = i < (NSInteger)[_styles count] ? _styles[i] : @"";
        NSSize sz = [_buttons[i] sizeWithAttributes:attrs];
        NSRect r = NSMakeRect(x - sz.width - 28, NSMaxY(p) - 40, sz.width + 28, 26);
        NSColor *fill = [style isEqualToString:@"go"] ? [NSColor controlAccentColor] : [style isEqualToString:@"danger"] ? PDFixed(0xc9433a) : PDColor(0xffffff, 0x3a3537);
        NSColor *ink = [style length] > 0 ? [NSColor whiteColor] : PDColor(0x1b1b1b, 0xeeeeee);

        PDFillRounded(r, 6, fill);
        if ([style length] == 0) {
            [PDAlpha(PDColor(0x000000, 0xffffff), .15) setStroke];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, .5, .5) xRadius:6 yRadius:6] stroke];
        }
        [_buttons[i] drawAtPoint:NSMakePoint(NSMinX(r) + 14, floor(NSMidY(r) - sz.height / 2)) withAttributes:PDText(PDFont(@"Inter-Regular", 12.5), ink)];
        [_rects insertObject:[NSValue valueWithRect:r] atIndex:0];
        x = NSMinX(r) - 8;
    }
}

- (void)finish:(NSInteger)i
{
    void (^done)(NSInteger, NSString *) = [[_done retain] autorelease];
    NSString *value = _field != nil ? [[[_field stringValue] copy] autorelease] : nil;

    [[self retain] autorelease];
    [self dismiss];
    if (done != nil)
        done(i, value);
}

- (void)fieldDone:(id)sender
{
    // return in the field presses the last button, the default one
    [self finish:[_buttons count] - 1];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    for (NSUInteger i = 0; i < [_rects count]; i++) {
        if (NSPointInRect(p, [_rects[i] rectValue])) {
            [self finish:i];
            return;
        }
    }
}

- (void)keyDown:(NSEvent *)event
{
    NSString *c = [event charactersIgnoringModifiers];

    if ([c isEqualToString:@"\e"])
        [self finish:0];
    else if ([c isEqualToString:@"\r"])
        [self finish:[_buttons count] - 1];
    else
        [super keyDown:event];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)dismiss
{
    NSWindow *w = [self window];

    [self removeFromSuperview];
    [w makeFirstResponder:[w contentView]];
}

@end

NSMenuItem *PDAddItem(NSMenu *menu, NSString *title, SEL action, NSString *key, NSUInteger mods)
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];

    if (mods != 0)
        [item setKeyEquivalentModifierMask:mods];
    [menu addItem:item];
    [item release];
    return item;
}

NSMenu *PDAddMenu(NSMenu *main, NSString *title)
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:title];

    [PDAddItem(main, title, NULL, @"", 0) setSubmenu:menu];
    [menu release];
    return menu;
}

// the app menu with About and Quit, and the Window menu, the caller adds its own menus between
NSMenu *PDStandardMenus(NSString *appName, NSMenu **windowMenu)
{
    NSMenu *main = [[[NSMenu alloc] initWithTitle:@"MainMenu"] autorelease];
    NSMenu *app = PDAddMenu(main, appName);

    [PDAddItem(app, [NSString stringWithFormat:NSLocalizedString(@"About %@", @""), appName], NULL, @"", 0) setEnabled:NO];
    [app addItem:[NSMenuItem separatorItem]];
    PDAddItem(app, [NSString stringWithFormat:NSLocalizedString(@"Hide %@", @""), appName], @selector(hide:), @"h", 0);
    PDAddItem(app, [NSString stringWithFormat:NSLocalizedString(@"Quit %@", @""), appName], @selector(terminate:), @"q", 0);
    if (windowMenu != NULL) {
        NSMenu *window = [[[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", @"")] autorelease];

        PDAddItem(window, NSLocalizedString(@"Minimize", @""), @selector(performMiniaturize:), @"m", 0);
        PDAddItem(window, NSLocalizedString(@"Zoom", @""), @selector(performZoom:), @"", 0);
        *windowMenu = window;
    }
    return main;
}

NSWindow *PDMakeWindow(NSSize size, NSSize minSize, CGFloat offset, id delegate)
{
    NSRect screen = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(0, 0, MIN(size.width, NSWidth(screen) - 32), MIN(size.height, NSHeight(screen) - 80));
    NSWindow *window;

    frame.origin.x = floor(NSMidX(screen) - NSWidth(frame) / 2 + offset);
    frame.origin.y = floor(NSMidY(screen) - NSHeight(frame) / 2 + 40 - offset);
    window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
                                                   NSResizableWindowMask | NSFullSizeContentViewWindowMask
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    [window setTitlebarAppearsTransparent:YES];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setToolbarStyle:NSWindowToolbarStyleUnified];
    [window setMinSize:minSize];
    [window setDelegate:delegate];
    [window setReleasedWhenClosed:NO];
    return [window autorelease];
}
