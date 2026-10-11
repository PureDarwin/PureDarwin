/*
 * main.m - Terminal: a shell in a window.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "TerminalView.h"
#import "TerminalTabs.h"

@interface TerminalController : NSObject <TerminalTabsOwner> {
    NSMutableArray *_windows;
}
@end

@implementation TerminalController

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

// the demo's menus: Terminal, Shell, Edit, View, Window, Help
- (void)buildMenu
{
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSUInteger shiftCmd = NSCommandKeyMask | NSShiftKeyMask;

    NSMenu *appMenu = addMenu(mainMenu, @"Terminal");
    [addItem(appMenu, @"About Terminal", NULL, @"") setEnabled:NO];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [addItem(appMenu, @"Settings\u2026", NULL, @",") setEnabled:NO];
    [appMenu addItem:[NSMenuItem separatorItem]];
    addItem(appMenu, @"Quit Terminal", @selector(terminate:), @"q");

    NSMenu *shellMenu = addMenu(mainMenu, @"Shell");
    [addItem(shellMenu, @"New Window", @selector(newWindow:), @"n") setTarget:self];
    addItem(shellMenu, @"New Tab", @selector(newTab:), @"t");
    [shellMenu addItem:[NSMenuItem separatorItem]];
    addItem(shellMenu, @"Close Tab", @selector(closeTab:), @"w");
    [addItem(shellMenu, @"Close Window", @selector(closeWindow:), @"w") setKeyEquivalentModifierMask:shiftCmd];

    NSMenu *editMenu = addMenu(mainMenu, @"Edit");
    addItem(editMenu, @"Copy", @selector(copy:), @"c");
    addItem(editMenu, @"Paste", @selector(paste:), @"v");
    addItem(editMenu, @"Select All", @selector(selectAll:), @"a");
    [editMenu addItem:[NSMenuItem separatorItem]];
    addItem(editMenu, @"Clear to Start", @selector(clearScrollback:), @"k");

    NSMenu *viewMenu = addMenu(mainMenu, @"View");
    addItem(viewMenu, @"Default Size", @selector(defaultFontSize:), @"0");
    addItem(viewMenu, @"Bigger", @selector(biggerFont:), @"+");
    addItem(viewMenu, @"Smaller", @selector(smallerFont:), @"-");
    [viewMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *fullScreen = addItem(viewMenu, @"Enter Full Screen", @selector(toggleFullScreen:), @"f");
    [fullScreen setKeyEquivalentModifierMask:NSCommandKeyMask | NSControlKeyMask];

    NSMenu *windowMenu = addMenu(mainMenu, @"Window");
    addItem(windowMenu, @"Minimize", @selector(performMiniaturize:), @"m");
    addItem(windowMenu, @"Zoom", @selector(performZoom:), @"");
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [addItem(windowMenu, @"Show Previous Tab", @selector(selectPreviousTab:), @"{") setKeyEquivalentModifierMask:shiftCmd];
    [addItem(windowMenu, @"Show Next Tab", @selector(selectNextTab:), @"}") setKeyEquivalentModifierMask:shiftCmd];

    NSMenu *helpMenu = addMenu(mainMenu, @"Help");
    [addItem(helpMenu, @"Terminal Help", NULL, @"") setEnabled:NO];

    [NSApp setMainMenu:mainMenu];
    [mainMenu release];
}

// each new window steps down and right from the last, as Terminal's cascade does
- (IBAction)newWindow:(id)sender
{
    TerminalTabs *tabs = [[[TerminalTabs alloc] initWithOwner:self] autorelease];
    NSWindow *window;
    NSSize size;

    if (![tabs addTab]) {
        if ([_windows count] == 0)
            [NSApp terminate:self];
        return;
    }
    size = [tabs preferredContentSize];
    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80 + 26 * [_windows count], 120, size.width, size.height)
                                         styleMask:NSTitledWindowMask | NSClosableWindowMask |
                                                   NSMiniaturizableWindowMask | NSResizableWindowMask
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    [window setReleasedWhenClosed:NO];
    [window setTitle:@"Terminal"];
    [window setDelegate:self];
    [window setAcceptsMouseMovedEvents:YES];
    [window setContentView:tabs];
    [_windows addObject:window];
    [window release];
    [window makeKeyAndOrderFront:nil];
}

- (void)terminalTabsDidClose:(TerminalTabs *)tabs
{
    [[tabs window] close];
}

// with no way back from the dock yet, Terminal quits with its last window
- (void)windowWillClose:(NSNotification *)note
{
    NSWindow *window = [note object];

    [[window retain] autorelease];
    [_windows removeObject:window];
    if ([_windows count] == 0)
        [NSApp terminate:self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    _windows = [[NSMutableArray alloc] init];
    [self buildMenu];
    [self newWindow:nil];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    TerminalController *controller = [[TerminalController alloc] init];

    [app setDelegate:controller];
    [app run];

    [controller release];
    [pool release];
    return 0;
}
