// AccountSheet.m - the small forms the Users & Groups pane opens: passwords, new users, pictures
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// drawn and typed into by hand like the Wi-Fi join sheet, so the two look the same

#import "AccountSheet.h"

#define SHEET_W 460
#define FIELD_X 170
#define PIC 52
#define PIC_GAP 12

static NSColor *rgba(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static NSDictionary *uiText(NSString *face, CGFloat size, NSColor *colour)
{
    return @{ NSFontAttributeName: [NSFont fontWithName:face size:size] ?: [NSFont systemFontOfSize:size], NSForegroundColorAttributeName: colour };
}

static BOOL uiDark(void)
{
    CFPropertyListRef style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);

    if (style == NULL)
        return NO;
    CFRelease(style);
    return YES;
}

// lines of text broken at spaces to fit a width
static NSArray *wrapped(NSString *text, NSDictionary *a, CGFloat width)
{
    NSMutableArray *lines = [NSMutableArray array];
    NSMutableString *line = [NSMutableString string];

    for (NSString *word in [text componentsSeparatedByString:@" "]) {
        NSString *next = [line length] ? [NSString stringWithFormat:@"%@ %@", line, word] : word;

        if ([line length] && [next sizeWithAttributes:a].width > width) {
            [lines addObject:[[line copy] autorelease]];
            [line setString:word];
        } else {
            [line setString:next];
        }
    }
    if ([line length])
        [lines addObject:line];
    return lines;
}

@interface AccountSheetView : NSView {
@public
    NSString *_title, *_message, *_check, *_button, *_error, *_picture;
    NSArray *_labels, *_choices, *_pictures;
    NSMutableArray *_values, *_secure, *_fieldRects, *_choiceRects, *_pictureRects;
    NSMutableDictionary *_images;
    NSColor *_accent;
    NSInteger _focus, _choice;
    BOOL _checked, _busy, _destructive;
    NSRect _checkRect, _okRect, _cancelRect;
    AccountSubmit _submit;
    void (^_done)(void);
}
- (CGFloat)layoutHeight;
@end

@implementation AccountSheetView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (NSColor *)fg
{
    return uiDark() ? rgba(0xee, 0xee, 0xee, 1) : rgba(0x1b, 0x1b, 0x1b, 1);
}

- (NSColor *)muted
{
    return uiDark() ? rgba(0x9a, 0x93, 0x95, 1) : rgba(0x6e, 0x6e, 0x6e, 1);
}

- (NSArray *)messageLines
{
    return _message ? wrapped(_message, uiText(@"Inter-Regular", 12, [self muted]), SHEET_W - 82 - 22) : @[];
}

- (CGFloat)headerHeight
{
    return MAX(96, 34 + 20 + 16 * [[self messageLines] count] + 26);
}

- (NSUInteger)pictureColumns
{
    return (SHEET_W - 44 + PIC_GAP) / (PIC + PIC_GAP);
}

// everything the sheet draws, top to bottom, so the window can be sized before it shows
- (CGFloat)layoutHeight
{
    CGFloat h = [self headerHeight] + 36 * [_labels count];

    if (_pictures)
        h += ((([_pictures count] + [self pictureColumns] - 1) / [self pictureColumns]) * (PIC + PIC_GAP)) + 8;
    if (_choices)
        h += 26 * [_choices count] + 6;
    if (_check)
        h += 28;
    return h + 26 + 56;
}

- (NSImage *)imageAt:(NSString *)path
{
    NSImage *img = _images[path];

    if (img == nil) {
        img = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        if (img != nil)
            _images[path] = img;
    }
    return img;
}

- (void)field:(NSRect)r text:(NSString *)t focused:(BOOL)f
{
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6];
    NSDictionary *a = uiText(@"Inter-Regular", 12.5, [self fg]);

    [(uiDark() ? rgba(0x1f, 0x1d, 0x1f, 1) : [NSColor whiteColor]) setFill];
    [p fill];
    [p setLineWidth:f ? 2 : 1];
    [(f ? [_accent colorWithAlphaComponent:0.7] : rgba(0, 0, 0, uiDark() ? 0.5 : 0.18)) setStroke];
    [p stroke];
    [t drawAtPoint:NSMakePoint(NSMinX(r) + 8, NSMinY(r) + 5) withAttributes:a];
    if (f) {
        CGFloat x = NSMinX(r) + 9 + [t sizeWithAttributes:a].width;

        [[self fg] setFill];
        NSRectFill(NSMakeRect(x, NSMinY(r) + 6, 1, NSHeight(r) - 12));
    }
}

- (void)mark:(NSRect)box on:(BOOL)on round:(BOOL)round
{
    NSBezierPath *p = round ? [NSBezierPath bezierPathWithOvalInRect:box] : [NSBezierPath bezierPathWithRoundedRect:box xRadius:3.5 yRadius:3.5];

    [(on ? _accent : (uiDark() ? rgba(0x1f, 0x1d, 0x1f, 1) : [NSColor whiteColor])) setFill];
    [p fill];
    if (!on) {
        [rgba(0, 0, 0, uiDark() ? 0.5 : 0.25) setStroke];
        [p stroke];
    } else if (round) {
        [[NSColor whiteColor] setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(box, 4.5, 4.5)] fill];
    } else {
        NSBezierPath *m = [NSBezierPath bezierPath];

        [m moveToPoint:NSMakePoint(NSMinX(box) + 3.5, NSMinY(box) + 7.5)];
        [m lineToPoint:NSMakePoint(NSMinX(box) + 6, NSMinY(box) + 10)];
        [m lineToPoint:NSMakePoint(NSMinX(box) + 10.5, NSMinY(box) + 4)];
        [m setLineWidth:1.6];
        [[NSColor whiteColor] setStroke];
        [m stroke];
    }
}

- (NSRect)button:(NSString *)t right:(CGFloat)right y:(CGFloat)y go:(BOOL)go enabled:(BOOL)en
{
    NSDictionary *a = uiText(@"Inter-Regular", 13, go ? [NSColor whiteColor] : [self fg]);
    NSSize sz = [t sizeWithAttributes:a];
    NSRect r = NSMakeRect(right - MAX(80, sz.width + 28), y, MAX(80, sz.width + 28), 26);
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6];
    NSColor *fill = go ? (_destructive ? rgba(0xd9, 0x4f, 0x45, 1) : _accent) : (uiDark() ? rgba(0x3a, 0x35, 0x37, 1) : [NSColor whiteColor]);

    [(en ? fill : [fill colorWithAlphaComponent:0.45]) setFill];
    [p fill];
    if (!go) {
        [rgba(0, 0, 0, 0.15) setStroke];
        [p stroke];
    }
    [t drawAtPoint:NSMakePoint(floor(NSMidX(r) - sz.width / 2), floor(NSMidY(r) - sz.height / 2)) withAttributes:a];
    return r;
}

// the badge: a head and shoulders in the accent, as the pane's own glyph
- (void)drawBadge:(NSRect)badge
{
    NSBezierPath *body = [NSBezierPath bezierPath];

    [_accent setFill];
    [[NSBezierPath bezierPathWithRoundedRect:badge xRadius:12 yRadius:12] fill];
    [[NSColor whiteColor] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMidX(badge) - 7, NSMinY(badge) + 9, 14, 14)] fill];
    [body appendBezierPathWithArcWithCenter:NSMakePoint(NSMidX(badge), NSMaxY(badge) - 6) radius:13 startAngle:180 endAngle:360 clockwise:NO];
    [body closePath];
    [body fill];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSDictionary *label = uiText(@"Inter-Regular", 12, [self fg]);
    CGFloat y = 34;

    [(uiDark() ? rgba(0x2c, 0x29, 0x2b, 1) : rgba(0xf6, 0xf2, 0xef, 1)) setFill];
    NSRectFill(b);
    [self drawBadge:NSMakeRect(22, 34, 46, 46)];
    [_title drawAtPoint:NSMakePoint(82, y) withAttributes:uiText(@"Inter-SemiBold", 13, [self fg])];
    y += 20;
    for (NSString *line in [self messageLines]) {
        [line drawAtPoint:NSMakePoint(82, y) withAttributes:uiText(@"Inter-Regular", 12, [self muted])];
        y += 16;
    }
    y = [self headerHeight];

    [_fieldRects removeAllObjects];
    for (NSUInteger i = 0; i < [_labels count]; i++) {
        NSString *v = _values[i];
        NSString *shown = [_secure[i] boolValue] ? [@"" stringByPaddingToLength:[v length] withString:@"•" startingAtIndex:0] : v;
        NSRect r = NSMakeRect(FIELD_X, y, SHEET_W - FIELD_X - 22, 26);
        NSSize ls = [_labels[i] sizeWithAttributes:label];

        [_labels[i] drawAtPoint:NSMakePoint(FIELD_X - 12 - ls.width, y + 5) withAttributes:label];
        [self field:r text:shown focused:(NSInteger)i == _focus];
        [_fieldRects addObject:[NSValue valueWithRect:r]];
        y += 36;
    }

    [_pictureRects removeAllObjects];
    if (_pictures) {
        NSUInteger cols = [self pictureColumns];

        for (NSUInteger i = 0; i < [_pictures count]; i++) {
            NSRect r = NSMakeRect(22 + (i % cols) * (PIC + PIC_GAP), y + (i / cols) * (PIC + PIC_GAP), PIC, PIC);
            NSImage *img = [self imageAt:_pictures[i]];

            if ([_picture isEqualToString:_pictures[i]]) {
                [_accent setFill];
                [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(r, -3.5, -3.5)] fill];
            }
            [NSGraphicsContext saveGraphicsState];
            [[NSBezierPath bezierPathWithOvalInRect:r] addClip];
            [rgba(0xc9, 0xc2, 0xbb, 1) setFill];
            [[NSBezierPath bezierPathWithOvalInRect:r] fill];
            [img drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
            [NSGraphicsContext restoreGraphicsState];
            [_pictureRects addObject:[NSValue valueWithRect:r]];
        }
        y += ((([_pictures count] + cols - 1) / cols) * (PIC + PIC_GAP)) + 8;
    }

    [_choiceRects removeAllObjects];
    for (NSUInteger i = 0; i < [_choices count]; i++) {
        NSRect r = NSMakeRect(FIELD_X - 60, y, SHEET_W - FIELD_X + 38, 20);

        [self mark:NSMakeRect(NSMinX(r), NSMinY(r) + 2, 14, 14) on:(NSInteger)i == _choice round:YES];
        [_choices[i] drawAtPoint:NSMakePoint(NSMinX(r) + 22, NSMinY(r) + 1) withAttributes:label];
        [_choiceRects addObject:[NSValue valueWithRect:r]];
        y += 26;
    }
    if (_choices)
        y += 6;

    _checkRect = NSZeroRect;
    if (_check) {
        _checkRect = NSMakeRect(FIELD_X, y, SHEET_W - FIELD_X - 22, 20);
        [self mark:NSMakeRect(NSMinX(_checkRect), NSMinY(_checkRect) + 2, 14, 14) on:_checked round:NO];
        [_check drawAtPoint:NSMakePoint(NSMinX(_checkRect) + 22, NSMinY(_checkRect) + 1) withAttributes:label];
        y += 28;
    }
    if (_error)
        [_error drawAtPoint:NSMakePoint(22, y) withAttributes:uiText(@"Inter-Regular", 12, rgba(0xd9, 0x4f, 0x45, 1))];

    CGFloat by = NSHeight(b) - 44;
    _okRect = [self button:_button right:NSWidth(b) - 22 y:by go:YES enabled:!_busy];
    _cancelRect = [self button:NSLocalizedString(@"Cancel", @"") right:NSMinX(_okRect) - 8 y:by go:NO enabled:!_busy];
    if (_busy)
        [NSLocalizedString(@"Working…", @"") drawAtPoint:NSMakePoint(22, by + 5) withAttributes:uiText(@"Inter-Regular", 12, [self muted])];
}

- (void)close
{
    [[self window] orderOut:nil];
    [[self window] autorelease];
}

- (void)go
{
    NSArray *values = [[_values copy] autorelease];
    BOOL checked = _checked;
    NSInteger choice = _choice;
    NSString *picture = [[_picture copy] autorelease];
    AccountSubmit submit = _submit;

    if (_busy)
        return;
    _busy = YES;
    [_error release];
    _error = nil;
    [self setNeedsDisplay:YES];
    [self retain];
    // the daemon hashes passwords and makes a wrong one wait, so the window keeps drawing meanwhile
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSString *error = [submit(values, checked, choice, picture) copy];

        dispatch_async(dispatch_get_main_queue(), ^{
            _busy = NO;
            if (error == nil) {
                if (_done)
                    _done();
                [self close];
            } else {
                _error = error;
                // a refused password is typed again
                for (NSUInteger i = 0; i < [_values count]; i++) {
                    if ([_secure[i] boolValue])
                        [_values[i] setString:@""];
                }
                for (NSUInteger i = 0; i < [_values count]; i++) {
                    if ([_secure[i] boolValue]) {
                        _focus = i;
                        break;
                    }
                }
                [self setNeedsDisplay:YES];
            }
            [self release];
        });
        [pool release];
    });
}

- (void)mouseDown:(NSEvent *)e
{
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];

    if (_busy)
        return;
    if (NSPointInRect(p, _cancelRect)) {
        [self close];
        return;
    }
    if (NSPointInRect(p, _okRect)) {
        [self go];
        return;
    }
    for (NSUInteger i = 0; i < [_fieldRects count]; i++) {
        if (NSPointInRect(p, [_fieldRects[i] rectValue]))
            _focus = i;
    }
    for (NSUInteger i = 0; i < [_choiceRects count]; i++) {
        if (NSPointInRect(p, [_choiceRects[i] rectValue]))
            _choice = i;
    }
    for (NSUInteger i = 0; i < [_pictureRects count]; i++) {
        if (NSPointInRect(p, NSInsetRect([_pictureRects[i] rectValue], -4, -4))) {
            [_picture release];
            _picture = [_pictures[i] copy];
        }
    }
    if (NSPointInRect(p, _checkRect))
        _checked = !_checked;
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)e
{
    NSString *k = [e characters];
    unichar c = [k length] ? [k characterAtIndex:0] : 0;
    NSMutableString *f = _focus < (NSInteger)[_values count] ? _values[_focus] : nil;

    if (_busy)
        return;
    if (c == '\r' || c == '\n' || c == 3)
        [self go];
    else if (c == 27)
        [self close];
    else if ((c == '\t' || c == 25) && [_values count] > 0)
        _focus = (_focus + (c == 25 ? [_values count] - 1 : 1)) % [_values count];
    else if (c == 0x7f || c == 8) {
        if ([f length])
            [f deleteCharactersInRange:NSMakeRange([f length] - 1, 1)];
    } else if (f != nil && c >= 0x20 && c < 0xF700 && [f length] < 128)
        [f appendString:k];
    [self setNeedsDisplay:YES];
}

- (void)dealloc
{
    // what was typed may be a password, so it's wiped before it goes
    for (NSMutableString *v in _values)
        [v setString:@""];
    [_title release];
    [_message release];
    [_check release];
    [_button release];
    [_error release];
    [_picture release];
    [_labels release];
    [_choices release];
    [_pictures release];
    [_values release];
    [_secure release];
    [_fieldRects release];
    [_choiceRects release];
    [_pictureRects release];
    [_images release];
    [_accent release];
    [_submit release];
    [_done release];
    [super dealloc];
}

@end

@implementation AccountSheet

+ (void)showTitle:(NSString *)title message:(NSString *)message fields:(NSArray *)fields
            check:(NSString *)check choices:(NSArray *)choices pictures:(NSArray *)pictures selected:(NSString *)selected
           button:(NSString *)button destructive:(BOOL)destructive accent:(NSColor *)accent
           submit:(AccountSubmit)submit done:(void (^)(void))done
{
    AccountSheetView *v = [[AccountSheetView alloc] initWithFrame:NSMakeRect(0, 0, SHEET_W, 200)];
    NSMutableArray *labels = [NSMutableArray array];
    NSRect screen = [[NSScreen mainScreen] visibleFrame], frame;
    NSWindow *w;
    CGFloat h;

    v->_values = [[NSMutableArray alloc] init];
    v->_secure = [[NSMutableArray alloc] init];
    for (NSArray *f in fields) {
        [labels addObject:f[0]];
        [v->_secure addObject:f[1]];
        [v->_values addObject:[NSMutableString stringWithString:[f count] > 2 ? f[2] : @""]];
    }
    v->_labels = [labels copy];
    v->_title = [title copy];
    v->_message = [message copy];
    v->_check = [check copy];
    v->_choices = [choices copy];
    v->_pictures = [pictures copy];
    v->_picture = [selected copy];
    v->_button = [button copy];
    v->_destructive = destructive;
    v->_accent = [accent retain];
    v->_submit = [submit copy];
    v->_done = [done copy];
    v->_fieldRects = [[NSMutableArray alloc] init];
    v->_choiceRects = [[NSMutableArray alloc] init];
    v->_pictureRects = [[NSMutableArray alloc] init];
    v->_images = [[NSMutableDictionary alloc] init];
    // the first empty field takes the typing
    for (NSUInteger i = 0; i < [v->_values count]; i++) {
        if ([v->_values[i] length] == 0) {
            v->_focus = i;
            break;
        }
    }
    h = [v layoutHeight];
    frame = NSMakeRect(NSMidX(screen) - SHEET_W / 2, NSMaxY(screen) - NSHeight(screen) * 0.18 - h, SHEET_W, h);
    w = [[NSWindow alloc] initWithContentRect:frame styleMask:NSTitledWindowMask | NSFullSizeContentViewWindowMask
                                      backing:NSBackingStoreBuffered defer:NO];
    [w setTitle:title];
    [w setTitlebarAppearsTransparent:YES];
    [w setTitleVisibility:NSWindowTitleHidden];
    [w setLevel:NSFloatingWindowLevel];
    // a full-size content view covers the title bar, so the frame is h and the lights sit on the sheet
    frame = [w frame];
    frame.origin.y += NSHeight(frame) - h;
    frame.size.height = h;
    [w setFrame:frame display:NO];
    [w setContentView:v];
    [v setFrame:NSMakeRect(0, 0, SHEET_W, h)];
    [w makeFirstResponder:v];
    [v release];
    [NSApp activateIgnoringOtherApps:YES];
    [w makeKeyAndOrderFront:nil];
}

@end
