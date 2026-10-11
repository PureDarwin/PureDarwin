/*
 * FinderView.m - one Finder window's contents: sidebar, toolbar, icon grid and status bar.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "FinderView.h"

// sizes from the demo's finder window
#define SIDE_W 180
#define TB_H 52
#define STATUS_H 24
#define GRID_PAD 14
#define CELL_MIN 92
#define COL_GAP 4
#define ROW_GAP 10
#define ICON 64
#define LABEL_H 18
#define ITEM_H (4 + ICON + 4 + LABEL_H + 4)
#define ROW_H 22
#define LINE_SCROLL 24
// the sidebar starts below the traffic lights, which sit over it
#define SIDE_TOP 40

enum { KindFolder, KindApp, KindFile };

@interface FinderEntry : NSObject {
@public
    NSString *name;
    NSString *full;
    NSString *icon;
    int kind;
}
@end

@implementation FinderEntry

- (void)dealloc
{
    [name release];
    [full release];
    [icon release];
    [super dealloc];
}

@end

static NSColor *fixed(int r, int g, int b)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1];
}

// the light palette and its dark counterpart, after the demo's dark Finder
static const struct { unsigned light, dark; } palette[] = {
    { 0x1b1b1b, 0xe6e6e6 }, { 0x8a8a8a, 0x969696 }, { 0x777777, 0x9a9a9a }, { 0x555555, 0xc4c4c4 },
    { 0xc4c4c4, 0x555555 }, { 0xe8e6e6, 0x302e30 }, { 0xd6d6d6, 0x141414 }, { 0xf6f6f6, 0x2c2c2e },
    { 0xe2e2e2, 0x141414 }, { 0xc8c8c8, 0x555555 }, { 0xfafafa, 0x3a3a3c },
};

static BOOL darkNow(void)
{
    return [[NSAppearance currentDrawingAppearance] _isDark];
}

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

// the file grid and status bar, the demo's #1f1f21
static NSColor *paper(void)
{
    return darkNow() ? fixed(0x1f, 0x1f, 0x21) : [NSColor whiteColor];
}

static NSFont *font(NSString *name, CGFloat size)
{
    NSFont *f = [NSFont fontWithName:name size:size];

    return f != nil ? f : [NSFont systemFontOfSize:size];
}

// the icon images, 128px for the grid and 32px for the sidebar, loaded once
static NSImage *iconImage(NSString *name, BOOL small)
{
    static NSMutableDictionary *cache;
    NSString *key = small ? [name stringByAppendingString:@"-small"] : name;
    NSImage *image;

    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    image = [cache objectForKey:key];
    if (image == nil) {
        NSString *png = [[NSBundle mainBundle] pathForResource:key ofType:@"png"];

        image = png != nil ? [[[NSImage alloc] initWithContentsOfFile:png] autorelease] : nil;
        if (image == nil)
            return nil;
        [cache setObject:image forKey:key];
    }
    return image;
}

// cuts a label to fit, ending it with an ellipsis
static NSString *fitted(NSString *text, NSDictionary *attrs, CGFloat width)
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

static NSString *iconForPath(NSString *path)
{
    NSString *home = NSHomeDirectory();
    NSDictionary *named = @{
        home: @"user-home",
        [home stringByAppendingPathComponent:@"Desktop"]: @"user-desktop",
        [home stringByAppendingPathComponent:@"Documents"]: @"folder-documents",
        [home stringByAppendingPathComponent:@"Downloads"]: @"folder-download",
        [home stringByAppendingPathComponent:@"Music"]: @"folder-music",
        [home stringByAppendingPathComponent:@"Pictures"]: @"folder-pictures",
        [home stringByAppendingPathComponent:@"Movies"]: @"folder-videos",
    };
    NSString *icon = [named objectForKey:path];

    return icon != nil ? icon : @"folder";
}

@implementation FinderView

- (instancetype)initWithFrame:(NSRect)frame path:(NSString *)path delegate:(id<FinderViewDelegate>)delegate
{
    NSString *home = NSHomeDirectory();

    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _delegate = delegate;
    _selection = [[NSMutableIndexSet alloc] init];
    _back = [[NSMutableArray alloc] init];
    _forward = [[NSMutableArray alloc] init];
    // a nil path marks a section heading
    _places = [@[
        @[ @"Favorites" ],
        @[ @"Home", home ],
        @[ @"Desktop", [home stringByAppendingPathComponent:@"Desktop"] ],
        @[ @"Documents", [home stringByAppendingPathComponent:@"Documents"] ],
        @[ @"Downloads", [home stringByAppendingPathComponent:@"Downloads"] ],
        @[ @"Applications", @"/Applications" ],
        @[ @"Locations" ],
        @[ @"PureDarwin", @"/" ],
    ] retain];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self showPath:path];
    return self;
}

- (void)dealloc
{
    [_path release];
    [_entries release];
    [_selection release];
    [_back release];
    [_forward release];
    [_places release];
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

- (NSString *)path
{
    return _path;
}

- (BOOL)sidebarHidden
{
    return _sidebarHidden;
}

// visible entries of a folder, folders and files mixed and sorted by name like Finder
- (NSArray *)entriesAtPath:(NSString *)path
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *names = [[fm contentsOfDirectoryAtPath:path error:NULL] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    NSMutableArray *entries = [NSMutableArray array];

    for (NSString *n in names) {
        FinderEntry *e;
        BOOL dir = NO;

        if ([n hasPrefix:@"."])
            continue;
        e = [[[FinderEntry alloc] init] autorelease];
        e->name = [n retain];
        e->full = [[path stringByAppendingPathComponent:n] retain];
        [fm fileExistsAtPath:e->full isDirectory:&dir];
        if (dir && [[n pathExtension] isEqualToString:@"app"]) {
            e->kind = KindApp;
            [e->name release];
            e->name = [[n stringByDeletingPathExtension] retain];
            if ([e->name isEqualToString:@"Terminal"])
                e->icon = @"terminal";
            else if ([e->name isEqualToString:@"Finder"])
                e->icon = @"files";
            else
                e->icon = @"settings";
        } else if (dir) {
            e->kind = KindFolder;
            e->icon = [iconForPath(e->full) retain];
        } else {
            e->kind = KindFile;
        }
        [entries addObject:e];
    }
    return entries;
}

- (void)showPath:(NSString *)path
{
    [_path release];
    _path = [path copy];
    [_entries release];
    _entries = [[self entriesAtPath:path] retain];
    [_selection removeAllIndexes];
    _scroll = 0;
    [self setNeedsDisplay:YES];
    [_delegate finderView:self didChangePath:_path];
}

- (void)goToPath:(NSString *)path
{
    BOOL dir = NO;

    path = [path stringByStandardizingPath];
    if ([path isEqualToString:_path] || ![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir] || !dir)
        return;
    if (_path != nil)
        [_back addObject:_path];
    [_forward removeAllObjects];
    [self showPath:path];
}

- (NSRect)sidebarRect
{
    return _sidebarHidden ? NSZeroRect : NSMakeRect(0, 0, SIDE_W, NSHeight([self bounds]));
}

- (CGFloat)mainX
{
    return _sidebarHidden ? 0 : SIDE_W + 1;
}

- (NSRect)gridRect
{
    NSRect b = [self bounds];
    CGFloat x = [self mainX];

    return NSMakeRect(x, TB_H + 1, NSWidth(b) - x, NSHeight(b) - TB_H - 1 - STATUS_H - 1);
}

- (NSRect)backButtonRect
{
    return NSMakeRect([self mainX] + 12, (TB_H - 24) / 2, 26, 24);
}

- (NSRect)forwardButtonRect
{
    return NSOffsetRect([self backButtonRect], 26 + 6, 0);
}

- (NSInteger)columns
{
    CGFloat inner = NSWidth([self gridRect]) - 2 * GRID_PAD;

    return MAX(1, (NSInteger)floor((inner + COL_GAP) / (CELL_MIN + COL_GAP)));
}

// an item's cell in view coordinates, already moved by the scroll offset
- (NSRect)itemRect:(NSUInteger)i
{
    NSRect grid = [self gridRect];
    NSInteger cols = [self columns];
    CGFloat inner = NSWidth(grid) - 2 * GRID_PAD;
    CGFloat w = floor((inner - (cols - 1) * COL_GAP) / cols);

    return NSMakeRect(NSMinX(grid) + GRID_PAD + (i % cols) * (w + COL_GAP),
                      NSMinY(grid) + GRID_PAD + (i / cols) * (ITEM_H + ROW_GAP) - _scroll, w, ITEM_H);
}

- (CGFloat)contentHeight
{
    NSInteger rows = ([_entries count] + [self columns] - 1) / [self columns];

    return 2 * GRID_PAD + rows * ITEM_H + MAX(0, rows - 1) * ROW_GAP;
}

- (void)clampScroll
{
    CGFloat max = MAX(0, [self contentHeight] - NSHeight([self gridRect]));

    _scroll = MIN(MAX(_scroll, 0), max);
}

- (void)drawChevron:(NSRect)r left:(BOOL)left enabled:(BOOL)enabled
{
    NSBezierPath *p = [NSBezierPath bezierPath];
    CGFloat cx = NSMidX(r), cy = NSMidY(r), d = left ? 1 : -1;

    [p moveToPoint:NSMakePoint(cx + 2.5 * d, cy - 6)];
    [p lineToPoint:NSMakePoint(cx - 3.5 * d, cy)];
    [p lineToPoint:NSMakePoint(cx + 2.5 * d, cy + 6)];
    [p setLineWidth:1.8];
    [(enabled ? rgb(0x55, 0x55, 0x55) : rgb(0xc4, 0xc4, 0xc4)) setStroke];
    [p stroke];
}

- (void)drawSidebar
{
    NSRect side = [self sidebarRect];
    NSDictionary *head = @{ NSFontAttributeName: font(@"Inter-SemiBold", 11), NSForegroundColorAttributeName: rgb(0x8a, 0x8a, 0x8a) };
    NSDictionary *row = @{ NSFontAttributeName: font(@"Inter-Regular", 13), NSForegroundColorAttributeName: rgb(0x1b, 0x1b, 0x1b) };
    CGFloat y = SIDE_TOP - 10;

    [rgb(0xe8, 0xe6, 0xe6) setFill];
    NSRectFill(side);
    [rgb(0xd6, 0xd6, 0xd6) setFill];
    NSRectFill(NSMakeRect(NSMaxX(side), 0, 1, NSHeight(side)));

    for (NSArray *place in _places) {
        if ([place count] == 1) {
            y += 10;
            [place[0] drawAtPoint:NSMakePoint(18, y) withAttributes:head];
            y += 14 + 4;
            continue;
        }
        NSRect r = NSMakeRect(10, y, SIDE_W - 20, ROW_H);

        if ([place[1] isEqualToString:_path]) {
            [[NSColor colorWithCalibratedWhite:darkNow() ? 1 : 0 alpha:0.1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6] fill];
        }
        [iconImage(iconForPath(place[1]), YES) drawInRect:NSMakeRect(NSMinX(r) + 8, NSMinY(r) + 3, 16, 16)
                                                 fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        [place[0] drawAtPoint:NSMakePoint(NSMinX(r) + 8 + 16 + 7, NSMinY(r) + 3) withAttributes:row];
        y += ROW_H;
    }
}

- (void)drawToolbar
{
    NSRect b = [self bounds];
    CGFloat x = [self mainX];
    NSDictionary *title = @{ NSFontAttributeName: font(@"Inter-Bold", 14), NSForegroundColorAttributeName: rgb(0x1b, 0x1b, 0x1b) };
    NSString *name = [_path isEqualToString:@"/"] ? @"PureDarwin" : [_path lastPathComponent];
    NSSize size = [name sizeWithAttributes:title];
    CGFloat tx = NSMaxX([self forwardButtonRect]) + 4 + 6;

    // the demo's gradient from #f9f9f9 to #f1f1f1 a row at a time, flat #f6f6f6 while inactive
    for (int row = 0; row < TB_H; row++) {
        if (![[self window] isKeyWindow]) {
            [rgb(0xf6, 0xf6, 0xf6) setFill];
            NSRectFill(NSMakeRect(x, 0, NSWidth(b) - x, TB_H));
            break;
        }
        [[NSColor colorWithCalibratedWhite:((darkNow() ? 0x33 : 0xf9) - (darkNow() ? 7.0 : 8.0) * row / (TB_H - 1)) / 255.0 alpha:1] setFill];
        NSRectFill(NSMakeRect(x, row, NSWidth(b) - x, 1));
    }
    [rgb(0xe2, 0xe2, 0xe2) setFill];
    NSRectFill(NSMakeRect(x, TB_H, NSWidth(b) - x, 1));

    [self drawChevron:[self backButtonRect] left:YES enabled:[_back count] > 0];
    [self drawChevron:[self forwardButtonRect] left:NO enabled:[_forward count] > 0];
    [fitted(name, title, NSWidth(b) - tx - 12) drawAtPoint:NSMakePoint(tx, floor((TB_H - size.height) / 2)) withAttributes:title];
}

// a plain page with a folded corner and the extension, like the demo's documents
- (void)drawDocument:(NSRect)r extension:(NSString *)ext
{
    NSRect page = NSMakeRect(NSMidX(r) - 24, NSMinY(r) + 4, 48, 56);
    NSDictionary *attrs = @{ NSFontAttributeName: font(@"SourceCodePro-Bold", 9), NSForegroundColorAttributeName: rgb(0x77, 0x77, 0x77) };
    NSSize size;

    ext = [[ext uppercaseString] length] > 0 ? fitted([ext uppercaseString], attrs, 40) : @"";
    size = [ext sizeWithAttributes:attrs];
    [rgb(0xc8, 0xc8, 0xc8) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:page xRadius:2 yRadius:2] fill];
    [rgb(0xfa, 0xfa, 0xfa) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(page, 1, 1) xRadius:1.5 yRadius:1.5] fill];
    [ext drawAtPoint:NSMakePoint(floor(NSMidX(page) - size.width / 2), NSMaxY(page) - 8 - size.height) withAttributes:attrs];
}

- (void)drawGrid
{
    NSRect grid = [self gridRect];
    NSDictionary *label = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: rgb(0x1b, 0x1b, 0x1b) };
    NSDictionary *picked = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSUInteger i;

    [paper() setFill];
    NSRectFill(grid);
    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:grid];
    for (i = 0; i < [_entries count]; i++) {
        FinderEntry *e = [_entries objectAtIndex:i];
        NSRect cell = [self itemRect:i];
        NSRect iconRect = NSMakeRect(floor(NSMidX(cell) - ICON / 2), NSMinY(cell) + 4, ICON, ICON);
        BOOL sel = [_selection containsIndex:i];
        NSDictionary *attrs = sel ? picked : label;
        NSString *text;
        NSSize size;
        NSRect pill;

        if (NSMaxY(cell) < NSMinY(grid) || NSMinY(cell) > NSMaxY(grid))
            continue;
        if (sel) {
            [[NSColor colorWithCalibratedWhite:darkNow() ? 1 : 0 alpha:0.08] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(iconRect, -4, -4) xRadius:6 yRadius:6] fill];
        }
        if (e->kind == KindFile)
            [self drawDocument:iconRect extension:[e->name pathExtension]];
        else
            [iconImage(e->icon, NO) drawInRect:iconRect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];

        text = fitted(e->name, attrs, NSWidth(cell) - 10);
        size = [text sizeWithAttributes:attrs];
        pill = NSMakeRect(floor(NSMidX(cell) - size.width / 2) - 5, NSMaxY(iconRect) + 4, ceil(size.width) + 10, LABEL_H);
        if (sel) {
            [[NSColor controlAccentColor] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:pill xRadius:4 yRadius:4] fill];
        }
        [text drawAtPoint:NSMakePoint(NSMinX(pill) + 5, NSMinY(pill) + floor((LABEL_H - size.height) / 2)) withAttributes:attrs];
    }

    // a thin overlay scroller while the folder is taller than the window
    if ([self contentHeight] > NSHeight(grid)) {
        CGFloat h = MAX(24, NSHeight(grid) * NSHeight(grid) / [self contentHeight]);
        CGFloat y = NSMinY(grid) + (NSHeight(grid) - h) * _scroll / ([self contentHeight] - NSHeight(grid));

        [[NSColor colorWithCalibratedWhite:0 alpha:0.28] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSMaxX(grid) - 8, y + 2, 5, h - 4) xRadius:2.5 yRadius:2.5] fill];
    }
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawStatus
{
    NSRect b = [self bounds];
    CGFloat x = [self mainX];
    NSDictionary *attrs = @{ NSFontAttributeName: font(@"Inter-Regular", 11), NSForegroundColorAttributeName: rgb(0x77, 0x77, 0x77) };
    NSUInteger n = [_entries count];
    NSString *text = [NSString stringWithFormat:@"%lu %@", (unsigned long)n, n == 1 ? @"item" : @"items"];
    NSSize size = [text sizeWithAttributes:attrs];
    CGFloat top = NSHeight(b) - STATUS_H;

    [paper() setFill];
    NSRectFill(NSMakeRect(x, top - 1, NSWidth(b) - x, STATUS_H + 1));
    [rgb(0xe2, 0xe2, 0xe2) setFill];
    NSRectFill(NSMakeRect(x, top - 1, NSWidth(b) - x, 1));
    [text drawAtPoint:NSMakePoint(floor(x + (NSWidth(b) - x - size.width) / 2), top + floor((STATUS_H - size.height) / 2))
       withAttributes:attrs];
}

- (void)drawRect:(NSRect)dirty
{
    // the content runs under the title bar, so it keeps to the frame's rounded corners
    [[NSBezierPath bezierPathWithRoundedRect:[self bounds] xRadius:8 yRadius:8] addClip];
    [self clampScroll];
    if (!_sidebarHidden)
        [self drawSidebar];
    [self drawToolbar];
    [self drawGrid];
    [self drawStatus];
}

- (NSInteger)itemAtPoint:(NSPoint)p
{
    NSUInteger i;

    if (!NSPointInRect(p, [self gridRect]))
        return -1;
    for (i = 0; i < [_entries count]; i++) {
        if (NSPointInRect(p, NSInsetRect([self itemRect:i], 6, 0)))
            return i;
    }
    return -1;
}

- (NSString *)placeAtPoint:(NSPoint)p
{
    CGFloat y = SIDE_TOP - 10;

    if (_sidebarHidden || p.x < 10 || p.x >= SIDE_W - 10)
        return nil;
    for (NSArray *place in _places) {
        if ([place count] == 1) {
            y += 10 + 14 + 4;
            continue;
        }
        if (p.y >= y && p.y < y + ROW_H)
            return place[1];
        y += ROW_H;
    }
    return nil;
}

- (void)openEntry:(FinderEntry *)e
{
    if (e->kind == KindFolder)
        [self goToPath:e->full];
    else if (e->kind == KindApp)
        [_delegate finderView:self openApplication:e->full];
    else
        NSLog(@"Finder: nothing opens %@ yet", e->full);
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSString *place = [self placeAtPoint:p];
    NSInteger i;


    if (place != nil) {
        [self goToPath:place];
        return;
    }
    if (NSPointInRect(p, [self backButtonRect])) {
        [self goBack:nil];
        return;
    }
    if (NSPointInRect(p, [self forwardButtonRect])) {
        [self goForward:nil];
        return;
    }
    // the toolbar and the sidebar's top stand in for the title bar
    if (p.y < TB_H || (!_sidebarHidden && p.x < SIDE_W && p.y < SIDE_TOP)) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    if (!NSPointInRect(p, [self gridRect]))
        return;

    i = [self itemAtPoint:p];
    [_selection removeAllIndexes];
    if (i >= 0)
        [_selection addIndex:i];
    [self setNeedsDisplay:YES];
    if (i >= 0 && [event clickCount] == 2)
        [self openEntry:[_entries objectAtIndex:i]];
}

- (void)scrollWheel:(NSEvent *)event
{
    // deltaY counts lines and is positive when scrolling up
    _scroll -= [event deltaY] * LINE_SCROLL;
    [self clampScroll];
    [self setNeedsDisplay:YES];
}

- (void)resizeWithOldSuperviewSize:(NSSize)old
{
    [super resizeWithOldSuperviewSize:old];
    [self clampScroll];
}

- (IBAction)goBack:(id)sender
{
    if ([_back count] == 0)
        return;
    [_forward addObject:_path];
    NSString *path = [[[_back lastObject] retain] autorelease];
    [_back removeLastObject];
    [self showPath:path];
}

- (IBAction)goForward:(id)sender
{
    if ([_forward count] == 0)
        return;
    [_back addObject:_path];
    NSString *path = [[[_forward lastObject] retain] autorelease];
    [_forward removeLastObject];
    [self showPath:path];
}

- (IBAction)goEnclosingFolder:(id)sender
{
    if (![_path isEqualToString:@"/"])
        [self goToPath:[_path stringByDeletingLastPathComponent]];
}

// Go menu items carry their folder as the represented object
- (IBAction)goToPlace:(id)sender
{
    [self goToPath:[sender representedObject]];
}

- (IBAction)openSelection:(id)sender
{
    NSUInteger i = [_selection firstIndex];

    if (i != NSNotFound)
        [self openEntry:[_entries objectAtIndex:i]];
}

- (IBAction)selectAll:(id)sender
{
    [_selection addIndexesInRange:NSMakeRange(0, [_entries count])];
    [self setNeedsDisplay:YES];
}

// "untitled folder", then "untitled folder 2" and on, selected once made
- (IBAction)newFolder:(id)sender
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *name = @"untitled folder";
    NSString *full = [_path stringByAppendingPathComponent:name];
    NSUInteger i;
    int n = 2;

    while ([fm fileExistsAtPath:full])
        full = [_path stringByAppendingPathComponent:[NSString stringWithFormat:@"%@ %d", name, n++]];
    if (![fm createDirectoryAtPath:full withIntermediateDirectories:NO attributes:nil error:NULL]) {
        NSBeep();
        return;
    }
    [_entries release];
    _entries = [[self entriesAtPath:_path] retain];
    [_selection removeAllIndexes];
    for (i = 0; i < [_entries count]; i++) {
        if ([((FinderEntry *)[_entries objectAtIndex:i])->full isEqualToString:full])
            [_selection addIndex:i];
    }
    [self setNeedsDisplay:YES];
}

- (IBAction)toggleSidebar:(id)sender
{
    _sidebarHidden = !_sidebarHidden;
    [self setNeedsDisplay:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    SEL action = [item action];

    if (action == @selector(goBack:))
        return [_back count] > 0;
    if (action == @selector(goForward:))
        return [_forward count] > 0;
    if (action == @selector(goEnclosingFolder:))
        return ![_path isEqualToString:@"/"];
    if (action == @selector(openSelection:))
        return [_selection count] > 0;
    if (action == @selector(toggleSidebar:))
        [item setTitle:_sidebarHidden ? @"Show Sidebar" : @"Hide Sidebar"];
    return YES;
}

@end
