/*
 * main.m - System Information: the hardware and software report behind About PureDarwin's More Info.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "Report.h"

#define SIDE_W 180
#define SIDE_TOP 40
#define TB_H 52
#define ROW_H 22
#define LINE_H 17
#define KEY_W 170
#define PAD 20
#define LINE_SCROLL 24

static NSColor *grey(int v)
{
    return [NSColor colorWithCalibratedWhite:v / 255.0 alpha:1];
}

static NSFont *font(NSString *name, CGFloat size)
{
    NSFont *f = [NSFont fontWithName:name size:size];

    return f != nil ? f : [NSFont systemFontOfSize:size];
}

@interface ReportView : NSView {
    NSArray *_report;
    NSMutableDictionary *_built;
    NSUInteger _current;
    CGFloat _scroll;
    CGFloat _contentHeight;
}
- (void)reload;
@end

@implementation ReportView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self reload];
    return self;
}

- (void)dealloc
{
    [_report release];
    [_built release];
    [super dealloc];
}

- (void)reload
{
    [_report release];
    _report = [PDReportSections() retain];
    [_built release];
    _built = [[NSMutableDictionary alloc] init];
    [self setNeedsDisplay:YES];
}

// a section's groups, gathered the first time it is shown and kept until Refresh
- (NSArray *)groupsAt:(NSUInteger)i
{
    NSString *name = _report[i][@"name"];
    NSArray *groups = _built[name];

    if (groups == nil) {
        groups = ((PDReportBuilder)_report[i][@"build"])();
        _built[name] = groups ?: @[];
    }
    return groups;
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

- (NSRect)mainRect
{
    NSRect b = [self bounds];

    return NSMakeRect(SIDE_W + 1, TB_H + 1, NSWidth(b) - SIDE_W - 1, NSHeight(b) - TB_H - 1);
}

- (void)drawSidebar
{
    NSDictionary *head = @{ NSFontAttributeName: font(@"Inter-SemiBold", 11), NSForegroundColorAttributeName: grey(0x8a) };
    NSDictionary *row = @{ NSFontAttributeName: font(@"Inter-Regular", 13), NSForegroundColorAttributeName: grey(0x1b) };
    CGFloat y = SIDE_TOP;
    NSUInteger i;

    [[NSColor colorWithCalibratedRed:0xe8 / 255.0 green:0xe6 / 255.0 blue:0xe6 / 255.0 alpha:1] setFill];
    NSRectFill(NSMakeRect(0, 0, SIDE_W, NSHeight([self bounds])));
    [grey(0xd6) setFill];
    NSRectFill(NSMakeRect(SIDE_W, 0, 1, NSHeight([self bounds])));
    [@"Report" drawAtPoint:NSMakePoint(18, y) withAttributes:head];
    y += 18;
    for (i = 0; i < [_report count]; i++) {
        NSRect r = NSMakeRect(10, y, SIDE_W - 20, ROW_H);

        if (i == _current) {
            [[NSColor colorWithCalibratedWhite:0 alpha:0.1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6] fill];
        }
        [_report[i][@"name"] drawAtPoint:NSMakePoint(NSMinX(r) + 10, NSMinY(r) + 3) withAttributes:row];
        y += ROW_H;
    }
}

- (void)drawToolbar
{
    NSRect b = [self bounds];
    NSDictionary *title = @{ NSFontAttributeName: font(@"Inter-Bold", 14), NSForegroundColorAttributeName: grey(0x1b) };
    NSString *heading = _report[_current][@"heading"];
    NSSize size = [heading sizeWithAttributes:title];
    int row;

    for (row = 0; row < TB_H; row++) {
        [[NSColor colorWithCalibratedWhite:([[self window] isKeyWindow] ? 0xf9 - 8.0 * row / (TB_H - 1) : 0xf6) / 255.0 alpha:1] setFill];
        NSRectFill(NSMakeRect(SIDE_W + 1, row, NSWidth(b) - SIDE_W - 1, 1));
    }
    [grey(0xe2) setFill];
    NSRectFill(NSMakeRect(SIDE_W + 1, TB_H, NSWidth(b) - SIDE_W - 1, 1));
    [heading drawAtPoint:NSMakePoint(SIDE_W + 1 + PAD, floor((TB_H - size.height) / 2)) withAttributes:title];
}

// lines a value needs at the given width, long kernel strings wrap instead of running off
- (int)linesFor:(NSString *)text width:(CGFloat)width attributes:(NSDictionary *)attrs
{
    CGFloat w = [text sizeWithAttributes:attrs].width;

    return MAX(1, MIN(6, (int)ceil(w / MAX(40, width))));
}

- (void)drawReport
{
    NSRect main = [self mainRect];
    NSDictionary *groupTitle = @{ NSFontAttributeName: font(@"Inter-SemiBold", 13), NSForegroundColorAttributeName: grey(0x1b) };
    NSDictionary *key = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: grey(0x6e) };
    NSDictionary *value = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: grey(0x1b) };
    NSDictionary *empty = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: grey(0x8a) };
    NSArray *groups = [self groupsAt:_current];
    CGFloat x = NSMinX(main) + PAD, y = NSMinY(main) + 16 - _scroll;
    CGFloat valueX = x + KEY_W + 12, valueW = NSMaxX(main) - PAD - valueX;

    [[NSColor whiteColor] setFill];
    NSRectFill(main);
    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:main];
    if ([groups count] == 0)
        [@"Nothing found." drawAtPoint:NSMakePoint(x, y) withAttributes:empty];
    for (NSDictionary *g in groups) {
        if (g[@"title"] != nil) {
            [g[@"title"] drawAtPoint:NSMakePoint(x, y) withAttributes:groupTitle];
            y += 22;
        }
        for (NSArray *row in g[@"rows"]) {
            int lines = [self linesFor:row[1] width:valueW attributes:value];
            NSSize size = [row[0] sizeWithAttributes:key];

            [[row[0] stringByAppendingString:@":"] drawAtPoint:NSMakePoint(x + KEY_W - size.width - 4, y) withAttributes:key];
            [row[1] drawInRect:NSMakeRect(valueX, y, valueW, lines * LINE_H) withAttributes:value];
            y += lines * LINE_H;
        }
        y += 14;
    }
    _contentHeight = y + _scroll - NSMinY(main) + 10;
    if (_contentHeight > NSHeight(main)) {
        CGFloat h = MAX(24, NSHeight(main) * NSHeight(main) / _contentHeight);
        CGFloat top = NSMinY(main) + (NSHeight(main) - h) * _scroll / (_contentHeight - NSHeight(main));

        [[NSColor colorWithCalibratedWhite:0 alpha:0.28] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSMaxX(main) - 8, top + 2, 5, h - 4) xRadius:2.5 yRadius:2.5] fill];
    }
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawRect:(NSRect)dirty
{
    [[NSBezierPath bezierPathWithRoundedRect:[self bounds] xRadius:8 yRadius:8] addClip];
    [self drawSidebar];
    [self drawToolbar];
    [self drawReport];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger i;

    if (p.x < SIDE_W && p.y >= SIDE_TOP + 18) {
        i = (NSInteger)((p.y - SIDE_TOP - 18) / ROW_H);
        if (i >= 0 && i < (NSInteger)[_report count] && (NSUInteger)i != _current) {
            _current = i;
            _scroll = 0;
            [self setNeedsDisplay:YES];
        }
        return;
    }
    if (p.y < TB_H || (p.x < SIDE_W && p.y < SIDE_TOP))
        [[self window] performWindowDragWithEvent:event];
}

- (void)scrollWheel:(NSEvent *)event
{
    CGFloat max = MAX(0, _contentHeight - NSHeight([self mainRect]));

    _scroll = MIN(MAX(_scroll - [event deltaY] * LINE_SCROLL, 0), max);
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event
{
    unichar c = [[event charactersIgnoringModifiers] length] > 0 ? [[event charactersIgnoringModifiers] characterAtIndex:0] : 0;

    if (c == NSUpArrowFunctionKey && _current > 0)
        _current--;
    else if (c == NSDownArrowFunctionKey && _current + 1 < [_report count])
        _current++;
    else {
        [super keyDown:event];
        return;
    }
    _scroll = 0;
    [self setNeedsDisplay:YES];
}

- (IBAction)refresh:(id)sender
{
    [self reload];
}

@end

@interface InfoController : NSObject {
    NSWindow *_window;
}
@end

@implementation InfoController

static NSMenuItem *addItem(NSMenu *menu, NSString *title, SEL action, NSString *key)
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];

    [menu addItem:item];
    [item release];
    return item;
}

static NSMenu *addMenu(NSMenu *mainMenu, NSString *title)
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:title];

    [addItem(mainMenu, title, NULL, @"") setSubmenu:menu];
    [menu release];
    return menu;
}

- (void)buildMenu
{
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenu *app = addMenu(mainMenu, @"System Information");
    NSMenu *file = addMenu(mainMenu, @"File");
    NSMenu *view = addMenu(mainMenu, @"View");
    NSMenu *window = addMenu(mainMenu, @"Window");

    addItem(app, @"Quit System Information", @selector(terminate:), @"q");
    addItem(file, @"Close", @selector(performClose:), @"w");
    addItem(view, @"Refresh", @selector(refresh:), @"r");
    addItem(window, @"Minimize", @selector(performMiniaturize:), @"m");
    addItem(window, @"Zoom", @selector(performZoom:), @"");
    [NSApp setMainMenu:mainMenu];
    [mainMenu release];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect frame = NSMakeRect(0, 0, 720, 480);
    ReportView *view;

    [self buildMenu];
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
                                                    NSResizableWindowMask | NSFullSizeContentViewWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"System Information"];
    [_window setTitlebarAppearsTransparent:YES];
    [_window setTitleVisibility:NSWindowTitleHidden];
    [_window setToolbarStyle:NSWindowToolbarStyleUnified];
    [_window setMinSize:NSMakeSize(520, 320)];
    [_window setDelegate:self];
    view = [[ReportView alloc] initWithFrame:frame];
    [_window setContentView:view];
    [_window makeFirstResponder:view];
    [view release];
    [_window makeKeyAndOrderFront:nil];
}

- (void)windowWillClose:(NSNotification *)note
{
    [NSApp terminate:self];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    InfoController *controller = [[InfoController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
