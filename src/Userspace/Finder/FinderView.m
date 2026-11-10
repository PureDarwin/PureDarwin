/*
 * FinderView.m - one Finder window's contents: sidebar, toolbar, icon grid and status bar.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "FinderView.h"
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>

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
// an absolute path is an app's own icon from its bundle
static NSImage *iconImage(NSString *name, BOOL small)
{
    static NSMutableDictionary *cache;
    NSString *key = small ? [name stringByAppendingString:@"-small"] : name;
    NSImage *image;

    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    image = [cache objectForKey:key];
    if (image == nil) {
        NSString *png = [name isAbsolutePath] ? name : [[NSBundle mainBundle] pathForResource:key ofType:@"png"];

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

NSString *FinderTrashPath(void)
{
    NSString *trash = [NSHomeDirectory() stringByAppendingPathComponent:@".Trash"];

    [[NSFileManager defaultManager] createDirectoryAtPath:trash withIntermediateDirectories:YES
                                               attributes:@{ NSFilePosixPermissions: @0700 } error:NULL];
    return trash;
}

void FinderShowError(NSWindow *window, NSString *message, NSError *error)
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    NSString *reason = [error localizedDescription];
    int code = [[error domain] isEqualToString:NSPOSIXErrorDomain] ? (int)[error code] : 0;

    // the errors people hit most get a plain sentence, anything else shows the system's reason
    if (code == EACCES || code == EPERM)
        reason = NSLocalizedString(@"You don’t have permission to do this.", @"");
    else if (code == ENOSPC)
        reason = NSLocalizedString(@"There isn’t enough space on the disk.", @"");
    else if (code == EEXIST)
        reason = NSLocalizedString(@"An item with that name already exists.", @"");
    else if (code == EROFS)
        reason = NSLocalizedString(@"The disk is read-only.", @"");
    [alert setAlertStyle:NSWarningAlertStyle];
    [alert setMessageText:message];
    [alert setInformativeText:reason ?: @""];
    [alert addButtonWithTitle:NSLocalizedString(@"OK", @"")];
    FinderRunSheet(alert, window, nil);
}

// a dialog shown above a Finder window: PD's AppKit crashes in both its app-modal run
// (_hideMenuViewIfNeeded) and its sheets (sheetOrderFrontFromFrame:), so Finder draws its own
@interface FinderDialog : NSObject {
@public
    NSPanel *panel;
    void (^done)(NSInteger);
}
@end

static NSMutableSet *openDialogs;

@implementation FinderDialog

- (void)pressed:(NSButton *)sender
{
    void (^handler)(NSInteger) = [done retain];

    [[self retain] autorelease];
    [panel orderOut:nil];
    [openDialogs removeObject:self];
    if (handler != nil)
        handler(NSAlertFirstButtonReturn + [sender tag]);
    [handler release];
}

- (void)dealloc
{
    [panel release];
    [done release];
    [super dealloc];
}

@end

static NSTextField *label(NSString *text, NSFont *f, NSRect frame)
{
    NSTextField *field = [[[NSTextField alloc] initWithFrame:frame] autorelease];

    [field setStringValue:text ?: @""];
    [field setFont:f];
    [field setBezeled:NO];
    [field setBordered:NO];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setDrawsBackground:NO];
    [[field cell] setWraps:YES];
    return field;
}

// lines a text needs at this width, counting its own line breaks
static CGFloat textHeight(NSString *text, NSFont *f, CGFloat width)
{
    NSDictionary *attrs = @{ NSFontAttributeName: f };
    CGFloat lineH = ceil([@"Ag" sizeWithAttributes:attrs].height) + 2;
    NSUInteger lines = 0;

    for (NSString *line in [text componentsSeparatedByString:@"\n"])
        lines += MAX(1, (NSUInteger)ceil([line sizeWithAttributes:attrs].width / width));
    return [text length] ? lines * lineH : 0;
}

void FinderRunSheet(NSAlert *alert, NSWindow *window, void (^done)(NSInteger))
{
    const CGFloat W = 380, pad = 20, gap = 8, buttonH = 28;
    NSFont *title = [NSFont boldSystemFontOfSize:13], *body = [NSFont systemFontOfSize:11];
    NSString *message = [alert messageText], *info = [alert informativeText];
    NSView *accessory = [alert accessoryView];
    NSArray *buttons = [alert buttons];
    CGFloat textW = W - 2 * pad, titleH = textHeight(message, title, textW), infoH = textHeight(info, body, textW);
    CGFloat accH = accessory ? NSHeight([accessory frame]) + gap : 0;
    CGFloat H = pad + titleH + (infoH ? gap + infoH : 0) + (accH ? gap + accH : 0) + gap * 2 + buttonH + pad;
    FinderDialog *dialog = [[[FinderDialog alloc] init] autorelease];
    NSView *content;
    NSRect frame;
    CGFloat y = H - pad, x = W - pad;

    window = window ?: [NSApp keyWindow] ?: [NSApp mainWindow];
    if (openDialogs == nil)
        openDialogs = [[NSMutableSet alloc] init];
    // centred over the window's top, where a sheet would hang
    frame = NSMakeRect(0, 0, W, H);
    if (window != nil) {
        frame.origin.x = floor(NSMidX([window frame]) - W / 2);
        frame.origin.y = floor(NSMaxY([window frame]) - 60 - H);
    } else {
        NSRect screen = [[NSScreen mainScreen] visibleFrame];
        frame.origin = NSMakePoint(floor(NSMidX(screen) - W / 2), floor(NSMidY(screen)));
    }
    dialog->panel = [[NSPanel alloc] initWithContentRect:frame styleMask:NSTitledWindowMask
                                                 backing:NSBackingStoreBuffered defer:NO];
    [dialog->panel setTitle:@""];
    [dialog->panel setLevel:NSFloatingWindowLevel];
    [dialog->panel setReleasedWhenClosed:NO];
    dialog->done = [done copy];
    content = [dialog->panel contentView];

    y -= titleH;
    [content addSubview:label(message, title, NSMakeRect(pad, y, textW, titleH))];
    if (infoH) {
        y -= gap + infoH;
        [content addSubview:label(info, body, NSMakeRect(pad, y, textW, infoH))];
    }
    if (accessory) {
        y -= gap + NSHeight([accessory frame]);
        [accessory setFrame:NSMakeRect(pad, y, textW, NSHeight([accessory frame]))];
        [content addSubview:accessory];
        // Return in a text field answers with the default button
        if ([accessory isKindOfClass:[NSTextField class]]) {
            [(NSTextField *)accessory setTag:0];
            [(NSTextField *)accessory setTarget:dialog];
            [(NSTextField *)accessory setAction:@selector(pressed:)];
        }
    }
    // buttons right to left in the order they were added, the first is the default
    for (NSUInteger i = 0; i < [buttons count]; i++) {
        NSString *t = [[buttons objectAtIndex:i] title];
        CGFloat bw = MAX(80, ceil([t sizeWithAttributes:@{ NSFontAttributeName: body }].width) + 28);
        NSButton *b = [[[NSButton alloc] initWithFrame:NSMakeRect(x - bw, pad, bw, buttonH)] autorelease];

        [b setTitle:t];
        [b setBezelStyle:NSRoundedBezelStyle];
        [b setTag:(NSInteger)i];
        [b setTarget:dialog];
        [b setAction:@selector(pressed:)];
        if (i == 0)
            [b setKeyEquivalent:@"\r"];
        else if ([t isEqualToString:NSLocalizedString(@"Cancel", @"")])
            [b setKeyEquivalent:@"\e"];
        [content addSubview:b];
        x -= bw + gap;
    }
    [openDialogs addObject:dialog];
    [dialog->panel makeKeyAndOrderFront:nil];
    if (accessory)
        [dialog->panel makeFirstResponder:accessory];
}

// where trashed items came from, so Put Back knows where to return them
static NSString *putBackListPath(void)
{
    return [FinderTrashPath() stringByAppendingPathComponent:@".PutBack.plist"];
}

static NSMutableDictionary *readPutBackList(void)
{
    NSMutableDictionary *list = [NSMutableDictionary dictionaryWithContentsOfFile:putBackListPath()];

    return list ?: [NSMutableDictionary dictionary];
}

// "name", then "name 2", "name 3" in folder, the extension kept at the end
static NSString *freeName(NSString *folder, NSString *name, NSString *suffix)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *ext = [name pathExtension], *base = [name stringByDeletingPathExtension];
    NSString *candidate;
    int n = 1;

    for (;;) {
        NSString *stem = base;

        if (suffix != nil)
            stem = n == 1 ? [NSString stringWithFormat:@"%@ %@", base, suffix] : [NSString stringWithFormat:@"%@ %@ %d", base, suffix, n];
        else if (n > 1)
            stem = [NSString stringWithFormat:@"%@ %d", base, n];
        candidate = [folder stringByAppendingPathComponent:[ext length] ? [stem stringByAppendingPathExtension:ext] : stem];
        if (![fm fileExistsAtPath:candidate])
            return candidate;
        n++;
    }
}

// one character per item, enabled or not, through every submenu
static NSString *menuState(NSMenu *menu)
{
    NSMutableString *state = [NSMutableString string];

    for (NSMenuItem *item in [menu itemArray]) {
        [state appendString:[item isEnabled] ? @"1" : @"0"];
        if ([item submenu] != nil)
            [state appendFormat:@"(%@)", menuState([item submenu])];
    }
    return state;
}

@interface NSApplication (FinderMenuBar)
- (oneway void)requestMenuUpdateForWindow:(NSNumber *)windowId;
@end

@implementation FinderView

// every selection or folder change redraws, which is also when the File menu's state changes.
// PD's menu bar only takes enabled states at publish time, so revalidate and publish again
- (void)setNeedsDisplay:(BOOL)flag
{
    static BOOL pending;

    [super setNeedsDisplay:flag];
    if (!flag || pending)
        return;
    pending = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        static NSString *published;
        NSString *state;

        pending = NO;
        [[NSApp mainMenu] update];
        state = menuState([NSApp mainMenu]);
        if ([state isEqualToString:published] || ![NSApp respondsToSelector:@selector(requestMenuUpdateForWindow:)])
            return;
        [published release];
        published = [state retain];
        [NSApp requestMenuUpdateForWindow:nil];
    });
}

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
        @[ NSLocalizedString(@"Favorites", @"") ],
        @[ NSLocalizedString(@"Home", @""), home ],
        @[ NSLocalizedString(@"Desktop", @""), [home stringByAppendingPathComponent:@"Desktop"] ],
        @[ NSLocalizedString(@"Documents", @""), [home stringByAppendingPathComponent:@"Documents"] ],
        @[ NSLocalizedString(@"Downloads", @""), [home stringByAppendingPathComponent:@"Downloads"] ],
        @[ NSLocalizedString(@"Applications", @""), @"/Applications" ],
        @[ NSLocalizedString(@"Locations", @"") ],
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
    return [self entriesAtPath:path error:NULL];
}

- (NSArray *)entriesAtPath:(NSString *)path error:(NSError **)error
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *names = [[fm contentsOfDirectoryAtPath:path error:error] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    NSMutableArray *entries = [NSMutableArray array];

    if (names == nil)
        return nil;

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
                e->icon = [[e->full stringByAppendingPathComponent:@"Contents/Resources/AppIcon.png"] retain];
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

    NSError *error = nil;

    path = [path stringByStandardizingPath];
    if ([path isEqualToString:_path] || ![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir] || !dir)
        return;
    // an unreadable folder reports the error
    if ([self entriesAtPath:path error:&error] == nil) {
        FinderShowError([self window], [NSString stringWithFormat:NSLocalizedString(@"The folder “%@” can’t be opened.", @""),
                                        [path lastPathComponent]], error);
        return;
    }
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
    NSString *name = [_path isEqualToString:@"/"] ? @"PureDarwin" : [self inTrash] ? NSLocalizedString(@"Trash", @"") : [_path lastPathComponent];
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
    NSString *text = [NSString stringWithFormat:n == 1 ? NSLocalizedString(@"%lu item", @"") : NSLocalizedString(@"%lu items", @""), (unsigned long)n];
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
    else if (![[NSWorkspace sharedWorkspace] openFile:e->full])
        NSBeep();
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
    // command toggles one item, shift extends from the first selected, a plain click picks one
    if (([event modifierFlags] & NSCommandKeyMask) && i >= 0) {
        if ([_selection containsIndex:i])
            [_selection removeIndex:i];
        else
            [_selection addIndex:i];
    } else if (([event modifierFlags] & NSShiftKeyMask) && i >= 0 && [_selection count] > 0) {
        NSUInteger from = MIN((NSUInteger)i, [_selection firstIndex]), to = MAX((NSUInteger)i, [_selection lastIndex]);

        [_selection addIndexesInRange:NSMakeRange(from, to - from + 1)];
    } else {
        [_selection removeAllIndexes];
        if (i >= 0)
            [_selection addIndex:i];
    }
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
    NSString *name = NSLocalizedString(@"untitled folder", @"");
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

// Return renames the selected item, as in Finder
- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event charactersIgnoringModifiers];
    NSUInteger mods = [event modifierFlags] & (NSCommandKeyMask | NSAlternateKeyMask | NSShiftKeyMask);
    unichar c = [chars length] == 1 ? [chars characterAtIndex:0] : 0;

    // the Delete key sends 0x7f, which PD's menus don't match against the \b equivalents
    if ((c == 0x7f || c == '\b') && mods == NSCommandKeyMask && [_selection count] > 0)
        [self moveToTrash:nil];
    else if ((c == 0x7f || c == '\b') && mods == (NSCommandKeyMask | NSAlternateKeyMask) && [_selection count] > 0)
        [self deleteImmediately:nil];
    else if ((c == 0x7f || c == '\b') && mods == (NSCommandKeyMask | NSShiftKeyMask))
        [NSApp sendAction:@selector(emptyTrash:) to:nil from:self];
    else if ([chars length] == 1 && ([chars characterAtIndex:0] == '\r' || [chars characterAtIndex:0] == 3) &&
        ([event modifierFlags] & NSCommandKeyMask) == 0 && [_selection count] == 1)
        [self renameSelection:nil];
    else
        [super keyDown:event];
}

- (NSArray *)selectedPaths
{
    NSMutableArray *paths = [NSMutableArray array];

    [_selection enumerateIndexesUsingBlock:^(NSUInteger i, BOOL *stop) {
        [paths addObject:((FinderEntry *)[_entries objectAtIndex:i])->full];
    }];
    return paths;
}

// rereads the folder after a change, keeping the given items selected
- (void)reloadSelecting:(NSArray *)paths
{
    NSError *error = nil;
    NSArray *entries = [self entriesAtPath:_path error:&error];
    NSUInteger i;

    // the folder itself went away or became unreadable, step out to the nearest one that works
    if (entries == nil) {
        NSString *up = _path;

        while (![up isEqualToString:@"/"] && (entries = [self entriesAtPath:(up = [up stringByDeletingLastPathComponent]) error:NULL]) == nil)
            ;
        [_path release];
        _path = [up copy];
        [_delegate finderView:self didChangePath:_path];
    }
    [_entries release];
    _entries = [entries ?: @[] retain];
    [_selection removeAllIndexes];
    for (i = 0; i < [_entries count]; i++) {
        if ([paths containsObject:((FinderEntry *)[_entries objectAtIndex:i])->full])
            [_selection addIndex:i];
    }
    [self setNeedsDisplay:YES];
}

- (void)reload
{
    [self reloadSelecting:[self selectedPaths]];
}

- (BOOL)inTrash
{
    return [[_path stringByStandardizingPath] isEqualToString:[FinderTrashPath() stringByStandardizingPath]];
}

- (IBAction)moveToTrash:(id)sender
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableDictionary *putBack = readPutBackList();
    NSString *trash = FinderTrashPath();

    // already in the Trash, the only way further is deleting for good
    if ([self inTrash]) {
        [self deleteImmediately:sender];
        return;
    }
    for (NSString *path in [self selectedPaths]) {
        NSString *dest = freeName(trash, [path lastPathComponent], nil);
        NSError *error = nil;

        if (![fm moveItemAtPath:path toPath:dest error:&error]) {
            FinderShowError([self window], [NSString stringWithFormat:NSLocalizedString(@"“%@” couldn’t be moved to the Trash.", @""),
                                            [path lastPathComponent]], error);
            break;
        }
        [putBack setObject:path forKey:[dest lastPathComponent]];
    }
    [putBack writeToFile:putBackListPath() atomically:YES];
    [self reloadSelecting:@[]];
}

- (IBAction)putBack:(id)sender
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableDictionary *putBack = readPutBackList();

    for (NSString *path in [self selectedPaths]) {
        NSString *origin = [putBack objectForKey:[path lastPathComponent]];
        NSError *error = nil;

        if (origin == nil)
            continue;
        // the original place may have a new item of the same name by now
        if ([fm fileExistsAtPath:origin])
            origin = freeName([origin stringByDeletingLastPathComponent], [origin lastPathComponent], nil);
        if (![fm moveItemAtPath:path toPath:origin error:&error]) {
            FinderShowError([self window], [NSString stringWithFormat:NSLocalizedString(@"“%@” couldn’t be put back.", @""),
                                            [path lastPathComponent]], error);
            break;
        }
        [putBack removeObjectForKey:[path lastPathComponent]];
    }
    [putBack writeToFile:putBackListPath() atomically:YES];
    [self reloadSelecting:@[]];
}

- (IBAction)deleteImmediately:(id)sender
{
    NSArray *paths = [self selectedPaths];
    NSAlert *alert;
    NSMutableDictionary *putBack = readPutBackList();

    if ([paths count] == 0)
        return;
    alert = [[[NSAlert alloc] init] autorelease];
    [alert setAlertStyle:NSWarningAlertStyle];
    [alert setMessageText:[paths count] == 1
        ? [NSString stringWithFormat:NSLocalizedString(@"Are you sure you want to delete “%@”?", @""), [paths[0] lastPathComponent]]
        : [NSString stringWithFormat:NSLocalizedString(@"Are you sure you want to delete these %lu items?", @""), (unsigned long)[paths count]]];
    [alert setInformativeText:NSLocalizedString(@"This item will be deleted immediately. You can’t undo this action.", @"")];
    [alert addButtonWithTitle:NSLocalizedString(@"Delete", @"")];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", @"")];
    FinderRunSheet(alert, [self window], ^(NSInteger code) {
        NSString *failed = nil;
        NSError *error = nil;

        if (code != NSAlertFirstButtonReturn)
            return;
        for (NSString *path in paths) {
            if (![[NSFileManager defaultManager] removeItemAtPath:path error:&error]) {
                failed = path;
                break;
            }
            [putBack removeObjectForKey:[path lastPathComponent]];
        }
        if ([self inTrash])
            [putBack writeToFile:putBackListPath() atomically:YES];
        [self reloadSelecting:@[]];
        // the confirmation sheet is still closing, the error gets the window next turn
        if (failed != nil) {
            NSString *message = [NSString stringWithFormat:NSLocalizedString(@"“%@” couldn’t be deleted.", @""), [failed lastPathComponent]];
            [error retain];
            dispatch_async(dispatch_get_main_queue(), ^{
                FinderShowError([self window], message, error);
                [error release];
            });
        }
    });
}

- (IBAction)renameSelection:(id)sender
{
    NSString *path = [[self selectedPaths] firstObject], *name, *dest;
    NSTextField *field;
    NSAlert *alert;
    NSError *error = nil;

    if (path == nil || [_selection count] != 1)
        return;
    field = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 22)] autorelease];
    // starts empty with the old name as placeholder, focusing a field holding text crashes
    // NSTextStorage_concrete, which lacks addAttributes:range:
    [[field cell] setPlaceholderString:[path lastPathComponent]];
    [field setFont:[NSFont systemFontOfSize:13]];
    alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"Rename “%@”", @""), [path lastPathComponent]]];
    [alert setInformativeText:NSLocalizedString(@"Enter a new name.", @"")];
    [alert setAccessoryView:field];
    [alert addButtonWithTitle:NSLocalizedString(@"Rename", @"")];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", @"")];
    FinderRunSheet(alert, [self window], ^(NSInteger code) {
        if (code != NSAlertFirstButtonReturn)
            return;
        // the result shows after the rename sheet has gone
        NSString *wanted = [[field stringValue] copy];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self finishRename:path to:wanted];
            [wanted release];
        });
    });
}

- (void)finishRename:(NSString *)path to:(NSString *)wanted
{
    NSString *name = [wanted stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]], *dest;
    NSError *error = nil;

    if ([name length] == 0 || [name isEqualToString:[path lastPathComponent]])
        return;
    if ([name rangeOfString:@"/"].location != NSNotFound || [name hasPrefix:@"."]) {
        NSAlert *bad = [[[NSAlert alloc] init] autorelease];

        [bad setMessageText:[NSString stringWithFormat:NSLocalizedString(@"The name “%@” can’t be used.", @""), name]];
        [bad setInformativeText:NSLocalizedString(@"Names can’t contain “/” or start with a dot.", @"")];
        [bad addButtonWithTitle:NSLocalizedString(@"OK", @"")];
        FinderRunSheet(bad, [self window], nil);
        return;
    }
    dest = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:name];
    if ([[NSFileManager defaultManager] fileExistsAtPath:dest]) {
        error = [NSError errorWithDomain:NSPOSIXErrorDomain code:EEXIST userInfo:nil];
    } else if ([[NSFileManager defaultManager] moveItemAtPath:path toPath:dest error:&error]) {
        [self reloadSelecting:@[ dest ]];
        return;
    }
    FinderShowError([self window], [NSString stringWithFormat:NSLocalizedString(@"“%@” couldn’t be renamed.", @""),
                                    [path lastPathComponent]], error);
}

// copies paths into the current folder, a name already there gets a number, or "copy" for a duplicate
- (NSArray *)copyPaths:(NSArray *)paths into:(NSString *)folder suffix:(NSString *)suffix move:(BOOL)move
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableArray *made = [NSMutableArray array];

    for (NSString *path in paths) {
        NSString *dest = [folder stringByAppendingPathComponent:[path lastPathComponent]];
        NSError *error = nil;
        BOOL ok;

        if (move && [[path stringByDeletingLastPathComponent] isEqualToString:folder])
            continue;
        // a folder can't go inside itself
        if ([[folder stringByAppendingString:@"/"] hasPrefix:[path stringByAppendingString:@"/"]]) {
            FinderShowError([self window], [NSString stringWithFormat:NSLocalizedString(@"“%@” can’t be copied into itself.", @""),
                                            [path lastPathComponent]], nil);
            break;
        }
        if (suffix != nil || [fm fileExistsAtPath:dest])
            dest = freeName(folder, [path lastPathComponent], suffix);
        ok = move ? [fm moveItemAtPath:path toPath:dest error:&error] : [fm copyItemAtPath:path toPath:dest error:&error];
        if (!ok) {
            FinderShowError([self window], [NSString stringWithFormat:move ? NSLocalizedString(@"“%@” couldn’t be moved.", @"")
                                                                           : NSLocalizedString(@"“%@” couldn’t be copied.", @""),
                                            [path lastPathComponent]], error);
            // a half-written copy is worse than none
            if (!move)
                [fm removeItemAtPath:dest error:NULL];
            break;
        }
        [made addObject:dest];
    }
    return made;
}

- (IBAction)duplicate:(id)sender
{
    [self reloadSelecting:[self copyPaths:[self selectedPaths] into:_path suffix:NSLocalizedString(@"copy", @"") move:NO]];
}

// the selected items go on the pasteboard as file names, as Finder puts them
- (IBAction)copy:(id)sender
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSArray *paths = [self selectedPaths];

    if ([paths count] == 0)
        return;
    [pb declareTypes:@[ NSFilenamesPboardType, NSStringPboardType ] owner:nil];
    [pb setPropertyList:paths forType:NSFilenamesPboardType];
    [pb setString:[paths componentsJoinedByString:@"\n"] forType:NSStringPboardType];
    // Paste becomes available
    [self setNeedsDisplay:YES];
}

- (NSArray *)pasteboardPaths
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    id list;

    // asking for a type the pasteboard doesn't hold logs an error, so look first
    if (![[pb types] containsObject:NSFilenamesPboardType])
        return nil;
    list = [pb propertyListForType:NSFilenamesPboardType];
    return [list isKindOfClass:[NSArray class]] ? list : nil;
}

- (IBAction)paste:(id)sender
{
    [self reloadSelecting:[self copyPaths:[self pasteboardPaths] into:_path suffix:nil move:NO]];
}

// Move Item Here, option-command-V, moves what was copied
- (IBAction)moveItemsHere:(id)sender
{
    [self reloadSelecting:[self copyPaths:[self pasteboardPaths] into:_path suffix:nil move:YES]];
}

static unsigned long long sizeOfPath(NSString *path)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *attrs = [fm attributesOfItemAtPath:path error:NULL];
    unsigned long long total;

    if (attrs == nil)
        return 0;
    total = [attrs fileSize];
    if ([[attrs fileType] isEqualToString:NSFileTypeDirectory]) {
        for (NSString *child in [fm contentsOfDirectoryAtPath:path error:NULL])
            total += sizeOfPath([path stringByAppendingPathComponent:child]);
    }
    return total;
}

- (IBAction)getInfo:(id)sender
{
    NSString *path = [[self selectedPaths] firstObject] ?: _path;
    NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL];
    NSDateFormatter *dates = [[[NSDateFormatter alloc] init] autorelease];
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    BOOL dir = [[attrs fileType] isEqualToString:NSFileTypeDirectory];
    unsigned long long size = dir ? sizeOfPath(path) : [attrs fileSize];
    NSString *kind = dir ? ([[path pathExtension] isEqualToString:@"app"] ? NSLocalizedString(@"Application", @"") : NSLocalizedString(@"Folder", @""))
                         : NSLocalizedString(@"Document", @"");

    // dates, owner and mode come from lstat, PD's attributesOfItemAtPath: leaves them out. ext4 has no birth time
    struct stat st;
    BOOL haveStat = lstat([path fileSystemRepresentation], &st) == 0;
    struct passwd *owner = haveStat ? getpwuid(st.st_uid) : NULL;

    [dates setDateStyle:NSDateFormatterLongStyle];
    [dates setTimeStyle:NSDateFormatterShortStyle];
    [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"%@ Info", @""), [path lastPathComponent]]];
    [alert setInformativeText:[NSString stringWithFormat:NSLocalizedString(@"Kind: %1$@\nSize: %2$@\nWhere: %3$@\nCreated: %4$@\nModified: %5$@\nOwner: %6$@\nPermissions: %7$@", @""),
        kind,
        [NSString stringWithFormat:NSLocalizedString(@"%llu bytes", @""), size],
        [path stringByDeletingLastPathComponent],
        haveStat && st.st_birthtimespec.tv_sec > 0 ? [dates stringFromDate:[NSDate dateWithTimeIntervalSince1970:st.st_birthtimespec.tv_sec]] : @"—",
        haveStat ? [dates stringFromDate:[NSDate dateWithTimeIntervalSince1970:st.st_mtimespec.tv_sec]] : @"—",
        owner ? [NSString stringWithUTF8String:owner->pw_name] : haveStat ? [NSString stringWithFormat:@"%u", st.st_uid] : @"—",
        haveStat ? [NSString stringWithFormat:@"%03o", st.st_mode & 07777] : @"—"]];
    [alert addButtonWithTitle:NSLocalizedString(@"OK", @"")];
    FinderRunSheet(alert, [self window], nil);
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
    if (action == @selector(openSelection:) || action == @selector(moveToTrash:) || action == @selector(deleteImmediately:) ||
        action == @selector(duplicate:) || action == @selector(copy:))
        return [_selection count] > 0;
    if (action == @selector(renameSelection:))
        return [_selection count] == 1;
    if (action == @selector(putBack:))
        return [self inTrash] && [_selection count] > 0;
    if (action == @selector(paste:) || action == @selector(moveItemsHere:))
        return [[self pasteboardPaths] count] > 0;
    if (action == @selector(toggleSidebar:))
        [item setTitle:_sidebarHidden ? NSLocalizedString(@"Show Sidebar", @"") : NSLocalizedString(@"Hide Sidebar", @"")];
    return YES;
}

@end
