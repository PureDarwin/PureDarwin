/*
 * main.m - Dock: the desktop picture and the dock bar.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>

@interface NSWindow (PDBlur)
- (void)_setBackgroundBlurInset:(CGFloat)inset radius:(CGFloat)radius;
@end

static NSString * const DesktopPictures = @"/System/Library/Desktop Pictures";

// fills the screen with the picture, cropped to keep its aspect like macOS's fill mode
@interface DesktopView : NSView {
    NSImage *_picture;
}
- (void)setPicture:(NSImage *)picture;
@end

@implementation DesktopView

- (void)setPicture:(NSImage *)picture
{
    [_picture release];
    _picture = [picture retain];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect bounds = [self bounds];
    NSSize size = [_picture size];
    CGFloat scale;
    NSRect from;

    [[NSColor colorWithCalibratedRed:0.85 green:0.47 blue:0.35 alpha:1] setFill];
    NSRectFill(bounds);
    if (_picture == nil || size.width <= 0 || size.height <= 0)
        return;

    scale = MAX(NSWidth(bounds) / size.width, NSHeight(bounds) / size.height);
    from.size = NSMakeSize(NSWidth(bounds) / scale, NSHeight(bounds) / scale);
    from.origin = NSMakePoint((size.width - from.size.width) / 2, (size.height - from.size.height) / 2);
    [_picture drawInRect:bounds fromRect:from operation:NSCompositeCopy fraction:1];
}

@end

// sizes from the demo's dock: 48px tiles, 6px gaps, 8px side padding, 16px corners
// the tile size and magnify switch come from Settings and can change while running
static CGFloat TILE = 48;
static BOOL magnify = YES;
#define GAP 6
#define PAD_X 8
#define PAD_TOP 6
#define PAD_BOTTOM 7
#define BAR_R 16
#define SEP_W 5
// room around the bar for its shadow, the magnified tile and the blur inset
#define MARGIN 16
#define SCREEN_GAP 6

@interface DockItem : NSObject {
@public
    NSString *label;
    NSString *path;
    NSString *executable;
    NSImage *icon;
    BOOL separator;
    BOOL running;
    NSRect frame;
}
@end

@implementation DockItem
@end

@class DockBarView;

@protocol DockBarDelegate
- (void)dockBar:(DockBarView *)bar clickedItem:(DockItem *)item;
- (void)dockBar:(DockBarView *)bar hoverItem:(DockItem *)item;
@end

@interface DockBarView : NSView {
    NSArray *_items;
    DockItem *_hover;
    id<DockBarDelegate> _delegate;
}
- (instancetype)initWithItems:(NSArray *)items delegate:(id<DockBarDelegate>)delegate;
- (NSRect)barRect;
- (void)setHover:(DockItem *)item;
@end

@implementation DockBarView

+ (NSSize)sizeForItems:(NSArray *)items
{
    CGFloat w = 2 * PAD_X + 2 + GAP * ((CGFloat)[items count] - 1);

    for (DockItem *item in items)
        w += item->separator ? SEP_W : TILE;
    return NSMakeSize(w + 2 * MARGIN, PAD_TOP + TILE + PAD_BOTTOM + 2 + 2 * MARGIN);
}

- (instancetype)initWithItems:(NSArray *)items delegate:(id<DockBarDelegate>)delegate
{
    NSSize size = [DockBarView sizeForItems:items];
    CGFloat x;

    if ((self = [super initWithFrame:NSMakeRect(0, 0, size.width, size.height)]) == nil)
        return nil;
    _items = [items retain];
    _delegate = delegate;
    x = MARGIN + 1 + PAD_X;
    for (DockItem *item in _items) {
        CGFloat w = item->separator ? SEP_W : TILE;

        item->frame = NSMakeRect(x, MARGIN + 1 + PAD_BOTTOM, w, TILE);
        x += w + GAP;
    }
    return self;
}

- (void)dealloc
{
    [_items release];
    [super dealloc];
}

- (NSRect)barRect
{
    return NSInsetRect([self bounds], MARGIN, MARGIN);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect bar = [self barRect];
    int i;

    // the window is clear outside the bar, NSRectFill copies so this resets every pixel
    [[NSColor clearColor] setFill];
    NSRectFill([self bounds]);

    // a soft shadow below the bar, the demo's 0 8px 30px at 18%
    for (i = 0; i < 8; i++) {
        NSRect r = NSOffsetRect(NSInsetRect(bar, -i - 1, -i - 1), 0, -4);

        [[NSColor colorWithCalibratedWhite:0 alpha:0.018] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:r xRadius:BAR_R + i yRadius:BAR_R + i] fill];
    }

    // the demo's dark dock, rgba(40,40,42,.35) inside a 15% white edge
    BOOL dark = [[NSAppearance currentDrawingAppearance] _isDark];

    [[NSColor colorWithCalibratedWhite:1 alpha:dark ? 0.15 : 0.4] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:BAR_R yRadius:BAR_R] fill];
    [(dark ? [NSColor colorWithCalibratedRed:40 / 255.0 green:40 / 255.0 blue:42 / 255.0 alpha:0.35]
           : [NSColor colorWithCalibratedWhite:1 alpha:0.28]) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(bar, 1, 1) xRadius:BAR_R - 1 yRadius:BAR_R - 1] fill];

    for (DockItem *item in _items) {
        NSRect r = item->frame;

        if (item->separator) {
            [[NSColor colorWithCalibratedWhite:dark ? 1 : 0 alpha:0.15] setFill];
            [[NSBezierPath bezierPathWithRect:NSMakeRect(NSMidX(r) - 0.5, NSMinY(bar) + 3, 1, NSHeight(bar) - 6)] fill];
            continue;
        }
        // the hovered tile lifts 6px and grows 12% like the demo's magnify
        if (item == _hover && magnify) {
            r = NSInsetRect(r, -TILE * 0.06, -TILE * 0.06);
            r.origin.y += 6;
        }
        [item->icon drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1];
        if (item->running) {
            [[NSColor colorWithCalibratedWhite:dark ? 1 : 0 alpha:dark ? 0.75 : 0.65] setFill];
            [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMidX(item->frame) - 2, NSMinY(item->frame) - 5, 4, 4)] fill];
        }
    }
}

- (DockItem *)itemAtPoint:(NSPoint)p
{
    for (DockItem *item in _items) {
        if (!item->separator && p.x >= NSMinX(item->frame) - GAP / 2 && p.x < NSMaxX(item->frame) + GAP / 2 &&
            NSPointInRect(p, [self barRect]))
            return item;
    }
    return nil;
}

- (void)setHover:(DockItem *)item
{
    if (item == _hover)
        return;
    _hover = item;
    [self setNeedsDisplay:YES];
    [_delegate dockBar:self hoverItem:item];
}

- (void)mouseMoved:(NSEvent *)event
{
    [self setHover:[self itemAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]]];
}

- (void)mouseExited:(NSEvent *)event
{
    [self setHover:nil];
}

- (void)mouseDown:(NSEvent *)event
{
    DockItem *item = [self itemAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    if (item != nil)
        [_delegate dockBar:self clickedItem:item];
}

@end

// the label above a hovered tile, its own small window so the bar stays compact
@interface DockTipView : NSView {
    NSString *_text;
}
- (void)setText:(NSString *)text;
+ (NSDictionary *)attributes;
@end

@implementation DockTipView

+ (NSDictionary *)attributes
{
    NSFont *font = [NSFont fontWithName:@"Inter-Regular" size:12];

    if (font == nil)
        font = [NSFont systemFontOfSize:12];
    return @{ NSFontAttributeName: font, NSForegroundColorAttributeName: [NSColor colorWithName:nil dynamicProvider:^NSColor *(NSAppearance *a) {
        return [NSColor colorWithCalibratedWhite:[a _isDark] ? 0.93 : 0.1 alpha:1];
    }] };
}

- (void)setText:(NSString *)text
{
    [_text release];
    _text = [text copy];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = NSInsetRect([self bounds], 4, 4);
    NSSize size = [_text sizeWithAttributes:[DockTipView attributes]];

    [[NSColor clearColor] setFill];
    NSRectFill([self bounds]);
    [[NSColor colorWithCalibratedWhite:0 alpha:0.06] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSOffsetRect(NSInsetRect(b, -1, -1), 0, -1) xRadius:6 yRadius:6] fill];
    [[NSColor colorWithCalibratedWhite:[[NSAppearance currentDrawingAppearance] _isDark] ? 0.17 : 0.94 alpha:0.92] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:5 yRadius:5] fill];
    [_text drawAtPoint:NSMakePoint(floor(NSMidX(b) - size.width / 2), floor(NSMidY(b) - size.height / 2))
        withAttributes:[DockTipView attributes]];
}

@end

@interface DockController : NSObject <DockBarDelegate> {
    NSWindow *_desktop;
    DesktopView *_view;
    NSWindow *_dock;
    DockBarView *_bar;
    NSWindow *_tip;
    DockTipView *_tipView;
    NSArray *_items;
    NSString *_picture;
}
@end

@implementation DockController

// read fresh each time, Settings writes these from its own process
- (id)pref:(NSString *)key
{
    CFPreferencesAppSynchronize(CFSTR("org.puredarwin.Dock"));
    return [(id)CFPreferencesCopyAppValue((CFStringRef)key, CFSTR("org.puredarwin.Dock")) autorelease];
}

// DesktopPicture is a name in Desktop Pictures or a full path, Sunset when unset
- (NSImage *)chosenPicture
{
    NSString *name = [self pref:@"DesktopPicture"];
    NSString *path;
    NSImage *image;

    if ([name length] == 0)
        name = @"Sunset";
    path = [name isAbsolutePath] ? name : [[DesktopPictures stringByAppendingPathComponent:name] stringByAppendingPathExtension:@"jpg"];
    image = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
    if (image == nil)
        NSLog(@"Dock: no desktop picture at %@", path);
    return image;
}

// covers the screen, retried until the compositor has reported its size
- (void)placeDesktop:(NSTimer *)timer
{
    NSRect screen = [[NSScreen mainScreen] frame];

    if (NSWidth(screen) < 100) {
        [NSTimer scheduledTimerWithTimeInterval:0.25 target:self selector:@selector(placeDesktop:) userInfo:nil repeats:NO];
        return;
    }
    // a tip left from before a resolution change would float at its old place
    [_tip orderOut:nil];
    [_desktop setFrame:screen display:YES];
    [_desktop orderFront:nil];
    [self placeDock];
}

// label, icon and bundle of each tile, nil marks the separator before the trash
- (NSArray *)makeItems
{
    NSArray *spec = @[
        @[ NSLocalizedString(@"Files", @""), @"files", @"/System/Library/CoreServices/Finder.app" ],
        @[ NSLocalizedString(@"Web", @""), @"web", @"/Applications/Web.app" ],
        @[ NSLocalizedString(@"Calculator", @""), @"calculator", @"/Applications/Calculator.app" ],
        @[ NSLocalizedString(@"Maps", @""), @"maps", @"/Applications/Maps.app" ],
        @[ NSLocalizedString(@"Settings", @""), @"settings", @"/Applications/Settings.app" ],
        @[ NSLocalizedString(@"Terminal", @""), @"terminal", @"/Applications/Utilities/Terminal.app" ],
        [NSNull null],
        @[ NSLocalizedString(@"Trash", @""), @"trash", @"" ],
    ];
    NSMutableArray *items = [NSMutableArray array];

    for (id entry in spec) {
        DockItem *item = [[[DockItem alloc] init] autorelease];

        if (entry == [NSNull null]) {
            item->separator = YES;
        } else {
            NSString *png = [[NSBundle mainBundle] pathForResource:entry[1] ofType:@"png"];

            item->label = [entry[0] retain];
            item->path = [entry[2] retain];
            item->icon = [[NSImage alloc] initWithContentsOfFile:png];
            if ([item->path length] > 0)
                item->executable = [[[[NSBundle bundleWithPath:item->path] executablePath] lastPathComponent] retain];
        }
        [items addObject:item];
    }
    return items;
}

// centred 6px above the bottom edge, the margin hangs below the screen
- (void)placeDock
{
    NSRect screen = [[NSScreen mainScreen] frame];
    NSRect frame = [_dock frame];

    frame.origin.x = floor(NSMidX(screen) - NSWidth(frame) / 2);
    frame.origin.y = NSMinY(screen) + SCREEN_GAP - MARGIN;
    [_dock setFrame:frame display:YES];
    [_dock orderFront:nil];
}

// the bar window is rebuilt whole when its tile size changes
- (void)makeBar
{
    NSSize size;

    if (_dock != nil) {
        [_dock orderOut:nil];
        [_dock release];
        [_bar release];
    }
    [_items release];
    _items = [[self makeItems] retain];
    _bar = [[DockBarView alloc] initWithItems:_items delegate:self];
    size = [_bar frame].size;
    _dock = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
                                        styleMask:NSBorderlessWindowMask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
    [_dock setLevel:NSDockWindowLevel];
    [_dock setOpaque:NO];
    [_dock setBackgroundColor:[NSColor clearColor]];
    [_dock setAcceptsMouseMovedEvents:YES];
    [_dock setContentView:_bar];
    [_dock _setBackgroundBlurInset:MARGIN radius:BAR_R];
    [_bar addTrackingRect:[_bar bounds] owner:_bar userData:NULL assumeInside:NO];
}

- (void)checkPrefs
{
    NSString *picture = [self pref:@"DesktopPicture"] ?: @"Sunset";
    id size = [self pref:@"TileSize"], mag = [self pref:@"Magnify"];
    CGFloat tile = size != nil ? MIN(MAX([size doubleValue], 32), 72) : 48;

    magnify = mag != nil ? [mag boolValue] : YES;
    if (![picture isEqualToString:_picture]) {
        [_picture release];
        _picture = [picture copy];
        [_view setPicture:[self chosenPicture]];
    }
    if (tile != TILE) {
        TILE = tile;
        [_tip orderOut:nil];
        [self makeBar];
        [self updateRunning:nil];
        [self placeDock];
    }
}

- (void)makeDock
{
    TILE = [self pref:@"TileSize"] != nil ? MIN(MAX([[self pref:@"TileSize"] doubleValue], 32), 72) : 48;
    [self makeBar];

    _tipView = [[DockTipView alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    _tip = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 30)
                                       styleMask:NSBorderlessWindowMask
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
    [_tip setLevel:NSPopUpMenuWindowLevel];
    [_tip setOpaque:NO];
    [_tip setBackgroundColor:[NSColor clearColor]];
    [_tip setIgnoresMouseEvents:YES];
    [_tip setContentView:_tipView];

    [self updateRunning:nil];
    [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(updateRunning:) userInfo:nil repeats:YES];
}

// a tile shows its dot while a process with the bundle's executable name is alive
- (void)updateRunning:(NSTimer *)timer
{
    int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
    struct kinfo_proc *procs = NULL;

    if (timer != nil)
        [self checkPrefs];
    size_t len = 0;
    NSMutableSet *names = [NSMutableSet set];
    BOOL changed = NO;

    if (sysctl(mib, 3, NULL, &len, NULL, 0) == 0 && (procs = malloc(len + 16 * sizeof(*procs))) != NULL) {
        len += 16 * sizeof(*procs);
        if (sysctl(mib, 3, procs, &len, NULL, 0) == 0) {
            for (size_t i = 0; i < len / sizeof(*procs); i++) {
                // an exited app that nobody has reaped yet is not running
                if (procs[i].kp_proc.p_stat == SZOMB)
                    continue;
                [names addObject:[NSString stringWithUTF8String:procs[i].kp_proc.p_comm]];
            }
        }
        free(procs);
    }
    for (DockItem *item in _items) {
        // p_comm keeps only the first MAXCOMLEN characters
        NSString *comm = [item->executable length] > MAXCOMLEN ? [item->executable substringToIndex:MAXCOMLEN] : item->executable;
        BOOL running = comm != nil && [names containsObject:comm];

        if (running != item->running) {
            item->running = running;
            changed = YES;
        }
    }
    if (changed)
        [_bar setNeedsDisplay:YES];
}

- (void)dockBar:(DockBarView *)bar clickedItem:(DockItem *)item
{
    if ([item->path length] == 0 || item->executable == nil) {
        NSLog(@"Dock: %@ is not installed", item->label);
        return;
    }
    // there is no way to bring a running app forward yet, so a second click does nothing
    if (item->running)
        return;
    [[NSWorkspace sharedWorkspace] launchApplication:item->path];
    item->running = YES;
    [_bar setNeedsDisplay:YES];
}

- (void)dockBar:(DockBarView *)bar hoverItem:(DockItem *)item
{
    NSSize size;
    NSRect frame;
    NSPoint tile;

    if (item == nil) {
        [_tip orderOut:nil];
        return;
    }
    size = [item->label sizeWithAttributes:[DockTipView attributes]];
    tile = [_dock convertBaseToScreen:NSMakePoint(NSMidX(item->frame), NSMinY(item->frame))];
    // the demo's tip sits 62px above the tile's bottom, 9px side and 3px vertical padding
    frame.size = NSMakeSize(ceil(size.width) + 18 + 8, ceil(size.height) + 6 + 8);
    frame.origin = NSMakePoint(floor(tile.x - frame.size.width / 2), tile.y + 62 - 4);
    [_tipView setText:item->label];
    [_tip setFrame:frame display:YES];
    [_tipView setFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
    [_tip orderFront:nil];
    [_tip display];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect frame = NSMakeRect(0, 0, 1024, 768);

    // a display mode change moves the screen edges
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(placeDesktop:)
                                                 name:NSApplicationDidChangeScreenParametersNotification object:nil];

    _desktop = [[NSWindow alloc] initWithContentRect:frame
                                           styleMask:NSBorderlessWindowMask
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
    [_desktop setLevel:NSDesktopWindowLevel];
    _view = [[DesktopView alloc] initWithFrame:frame];
    [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_view setPicture:[self chosenPicture]];
    [_desktop setContentView:_view];
    [self makeDock];
    [self placeDesktop:nil];
}

@end

// a crash prints where it happened, the kernel alone only reports the signal
static void crashed(int sig, siginfo_t *info, void *context)
{
    void *frames[48];
    int count = backtrace(frames, 48);

    ucontext_t *uc = context;
    void *pc = (void *)uc->uc_mcontext->__ss.__rip;
    Dl_info at;

    fprintf(stderr, "Dock: signal %d at %p\n", sig, info->si_addr);
    // the faulting instruction first, the backtrace below may not survive a corrupted stack
    if (dladdr(pc, &at) && at.dli_sname != NULL)
        fprintf(stderr, "  pc %s + %ld (%s)\n", at.dli_sname, (long)((char *)pc - (char *)at.dli_saddr), at.dli_fname);
    else
        fprintf(stderr, "  pc %p\n", pc);
    // callers from the saved frame pointers
    void **fp = (void **)uc->uc_mcontext->__ss.__rbp;

    for (int i = 0; i < 16 && fp != NULL && ((uintptr_t)fp & 7) == 0; i++) {
        void *ret = fp[1];

        if (dladdr(ret, &at) && at.dli_sname != NULL)
            fprintf(stderr, "  fp%-2d %s + %ld\n", i, at.dli_sname, (long)((char *)ret - (char *)at.dli_saddr));
        else
            fprintf(stderr, "  fp%-2d %p\n", i, ret);
        if ((void **)fp[0] <= fp)
            break;
        fp = (void **)fp[0];
    }
    fflush(stderr);
    for (int i = 0; i < count; i++) {
        Dl_info where;

        if (dladdr(frames[i], &where) && where.dli_sname != NULL)
            fprintf(stderr, "  %2d %s + %ld (%s)\n", i, where.dli_sname, (long)((char *)frames[i] - (char *)where.dli_saddr),
                    where.dli_fname ? where.dli_fname : "?");
        else
            fprintf(stderr, "  %2d %p\n", i, frames[i]);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, const char *argv[])
{
    struct sigaction action = { 0 };
    // its own stack, so a stack overflow can still be reported
    stack_t altstack = { .ss_sp = malloc(1 << 16), .ss_size = 1 << 16 };

    sigaltstack(&altstack, NULL);
    action.sa_sigaction = crashed;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    // the apps the Dock launches are its children, ignoring SIGCHLD lets the kernel reap them as they quit
    signal(SIGCHLD, SIG_IGN);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    DockController *controller;

    [NSApplication sharedApplication];
    controller = [[DockController alloc] init];
    [NSApp setDelegate:controller];
    [NSApp run];
    [pool release];
    return 0;
}
