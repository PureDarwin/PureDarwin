/*
 * main.m - Finder: browse folders and open apps.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "FinderView.h"

@interface FinderController : NSObject <FinderViewDelegate> {
    NSMutableArray *_windows;
}
@end

@implementation FinderController

static NSMenuItem *addItem(NSMenu *menu, NSString *title, SEL action, NSString *key, NSUInteger mods)
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];

    if (mods != 0)
        [item setKeyEquivalentModifierMask:mods];
    [menu addItem:item];
    [item release];
    return item;
}

static NSMenu *addMenu(NSMenu *mainMenu, NSString *title)
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:title];

    [addItem(mainMenu, title, NULL, @"", 0) setSubmenu:menu];
    [menu release];
    return menu;
}

static void addDisabled(NSMenu *menu, NSString *title, NSString *key, NSUInteger mods)
{
    [addItem(menu, title, NULL, key, mods) setEnabled:NO];
}

- (void)buildMenu
{
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSString *home = NSHomeDirectory();
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;

    NSMenu *app = addMenu(mainMenu, @"Finder");
    addDisabled(app, @"About Finder", @"", 0);
    [app addItem:[NSMenuItem separatorItem]];
    addDisabled(app, @"Settings…", @",", 0);
    [app addItem:[NSMenuItem separatorItem]];
    addDisabled(app, @"Empty Trash…", @"\b", shift);
    [app addItem:[NSMenuItem separatorItem]];
    addItem(app, @"Quit Finder", @selector(terminate:), @"q", 0);

    NSMenu *file = addMenu(mainMenu, @"File");
    [addItem(file, @"New Finder Window", @selector(newWindow:), @"n", 0) setTarget:self];
    addItem(file, @"New Folder", @selector(newFolder:), @"n", shift);
    [file addItem:[NSMenuItem separatorItem]];
    addItem(file, @"Open", @selector(openSelection:), @"o", 0);
    [file addItem:[NSMenuItem separatorItem]];
    addItem(file, @"Close Window", @selector(performClose:), @"w", 0);

    NSMenu *edit = addMenu(mainMenu, @"Edit");
    addDisabled(edit, @"Undo", @"z", 0);
    [edit addItem:[NSMenuItem separatorItem]];
    addDisabled(edit, @"Copy", @"c", 0);
    addDisabled(edit, @"Paste", @"v", 0);
    addItem(edit, @"Select All", @selector(selectAll:), @"a", 0);

    NSMenu *view = addMenu(mainMenu, @"View");
    [addItem(view, @"as Icons", NULL, @"1", 0) setState:NSOnState];
    addDisabled(view, @"as List", @"2", 0);
    [view addItem:[NSMenuItem separatorItem]];
    addItem(view, @"Hide Sidebar", @selector(toggleSidebar:), @"s", cmd | NSControlKeyMask);

    NSMenu *go = addMenu(mainMenu, @"Go");
    addItem(go, @"Back", @selector(goBack:), @"[", 0);
    addItem(go, @"Forward", @selector(goForward:), @"]", 0);
    addItem(go, @"Enclosing Folder", @selector(goEnclosingFolder:), [NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey], 0);
    [go addItem:[NSMenuItem separatorItem]];
    NSArray *places = @[
        @[ @"Documents", [home stringByAppendingPathComponent:@"Documents"], @"o" ],
        @[ @"Desktop", [home stringByAppendingPathComponent:@"Desktop"], @"d" ],
        @[ @"Downloads", [home stringByAppendingPathComponent:@"Downloads"], @"l" ],
        @[ @"Home", home, @"h" ],
        @[ @"Computer", @"/", @"c" ],
        @[ @"Applications", @"/Applications", @"a" ],
    ];
    for (NSArray *place in places) {
        NSUInteger mods = [place[0] isEqualToString:@"Downloads"] ? cmd | NSAlternateKeyMask : shift;

        [addItem(go, place[0], @selector(goToPlace:), place[2], mods) setRepresentedObject:place[1]];
    }

    NSMenu *window = addMenu(mainMenu, @"Window");
    addItem(window, @"Minimize", @selector(performMiniaturize:), @"m", 0);
    addItem(window, @"Zoom", @selector(performZoom:), @"", 0);

    [NSApp setMainMenu:mainMenu];
    [mainMenu release];
}

// the folders Finder's sidebar points at, made on first run like a new account's
- (void)makeHomeFolders
{
    NSString *home = NSHomeDirectory();

    for (NSString *name in @[ @"Desktop", @"Documents", @"Downloads" ])
        [[NSFileManager defaultManager] createDirectoryAtPath:[home stringByAppendingPathComponent:name]
                                  withIntermediateDirectories:YES attributes:nil error:NULL];
}

- (void)openWindowAtPath:(NSString *)path
{
    NSRect screen = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(0, 0, MIN(760, NSWidth(screen) - 32), MIN(460, NSHeight(screen) - 100));
    NSWindow *window;
    FinderView *view;

    // centred a little right and up like the demo, each new window stepped down from the last
    frame.origin.x = floor(NSMidX(screen) - NSWidth(frame) / 2 + 60 + 24 * [_windows count]);
    frame.origin.y = floor(NSMidY(screen) - NSHeight(frame) / 2 + 30 - 24 * [_windows count]);
    window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:NSTitledWindowMask | NSClosableWindowMask |
                                                   NSMiniaturizableWindowMask | NSResizableWindowMask |
                                                   NSFullSizeContentViewWindowMask
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    // one unified bar like the demo, the sidebar and toolbar drawn where the title bar was
    [window setTitlebarAppearsTransparent:YES];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setToolbarStyle:NSWindowToolbarStyleUnified];
    [window setMinSize:NSMakeSize(420, 260)];
    [window setDelegate:self];
    [window setReleasedWhenClosed:NO];
    view = [[FinderView alloc] initWithFrame:[[window contentView] bounds] path:path delegate:self];
    [window setContentView:view];
    [window setTitle:[path isEqualToString:@"/"] ? @"PureDarwin" : [path lastPathComponent]];
    [window makeFirstResponder:view];
    [view release];
    [_windows addObject:window];
    [window release];
    [window makeKeyAndOrderFront:nil];
}

- (IBAction)newWindow:(id)sender
{
    [self openWindowAtPath:NSHomeDirectory()];
}

- (void)finderView:(FinderView *)view didChangePath:(NSString *)path
{
    [[view window] setTitle:[path isEqualToString:@"/"] ? @"PureDarwin" : [path lastPathComponent]];
}

- (void)finderView:(FinderView *)view openApplication:(NSString *)path
{
    if ([[path lastPathComponent] isEqualToString:@"Finder.app"])
        [self newWindow:nil];
    else
        [[NSWorkspace sharedWorkspace] launchApplication:path];
}

// with no way yet to bring Finder back from the dock, it quits with its last window
- (void)windowWillClose:(NSNotification *)note
{
    [[[note object] retain] autorelease];
    [_windows removeObject:[note object]];
    if ([_windows count] == 0)
        [NSApp terminate:self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    _windows = [[NSMutableArray alloc] init];
    [self buildMenu];
    [self makeHomeFolders];
    [self newWindow:nil];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    FinderController *controller = [[FinderController alloc] init];

    [app setDelegate:controller];
    [app run];

    [controller release];
    [pool release];
    return 0;
}
