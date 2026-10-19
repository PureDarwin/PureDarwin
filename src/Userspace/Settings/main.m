/*
 * main.m - Settings: PureDarwin's own system settings.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "SettingsView.h"
#import "SettingsData.h"

@interface SettingsController : NSObject {
    NSWindow *_window;
    SettingsView *_view;
}
@end

@implementation SettingsController

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
    NSMenu *app = addMenu(mainMenu, NSLocalizedString(@"Settings", @""));
    NSMenu *edit = addMenu(mainMenu, NSLocalizedString(@"Edit", @""));
    NSMenu *view = addMenu(mainMenu, NSLocalizedString(@"View", @""));
    NSMenu *window = addMenu(mainMenu, NSLocalizedString(@"Window", @""));
    NSMenu *help = addMenu(mainMenu, NSLocalizedString(@"Help", @""));

    [addItem(app, NSLocalizedString(@"About Settings", @""), NULL, @"") setEnabled:NO];
    [app addItem:[NSMenuItem separatorItem]];
    addItem(app, NSLocalizedString(@"Quit Settings", @""), @selector(terminate:), @"q");
    [addItem(edit, NSLocalizedString(@"Copy", @""), NULL, @"c") setEnabled:NO];
    [addItem(edit, NSLocalizedString(@"Paste", @""), NULL, @"v") setEnabled:NO];
    addItem(view, NSLocalizedString(@"Back", @""), @selector(goBack:), @"[");
    addItem(view, NSLocalizedString(@"Show All", @""), @selector(goHome:), @"l");
    [view addItem:[NSMenuItem separatorItem]];
    addItem(view, NSLocalizedString(@"Search", @""), @selector(find:), @"f");
    addItem(window, NSLocalizedString(@"Minimize", @""), @selector(performMiniaturize:), @"m");
    addItem(window, NSLocalizedString(@"Zoom", @""), @selector(performZoom:), @"");
    [addItem(help, NSLocalizedString(@"Settings Help", @""), NULL, @"") setEnabled:NO];
    [NSApp setMainMenu:mainMenu];
    [mainMenu release];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{

    NSRect frame = NSMakeRect(0, 0, 760, 580);
    NSString *open = PDPref(@"org.puredarwin.Settings", @"OpenPane");
    NSArray *args = [[NSProcessInfo processInfo] arguments];

    // "Settings session" opens a pane from a shell
    if ([args count] > 1 && ![args[1] hasPrefix:@"-"])
        open = args[1];

    [self buildMenu];
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
                                                    NSResizableWindowMask | NSFullSizeContentViewWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:NSLocalizedString(@"Settings", @"")];
    [_window setTitlebarAppearsTransparent:YES];
    [_window setTitleVisibility:NSWindowTitleHidden];
    [_window setToolbarStyle:NSWindowToolbarStyleUnifiedCompact];
    [_window setMinSize:NSMakeSize(560, 400)];
    [_window setDelegate:self];
    _view = [[SettingsView alloc] initWithFrame:frame];
    [_window setContentView:_view];
    [_window makeFirstResponder:_view];
    // another app can ask for a pane by leaving its name here before launching Settings
    if ([open length] > 0) {
        [_view showPane:open];
        PDSetPref(@"org.puredarwin.Settings", @"OpenPane", nil);
    }
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
    SettingsController *controller = [[SettingsController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
