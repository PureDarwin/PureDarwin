// main.m - an AppKit window in a web page: a label, a text field and a button that counts its clicks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <AppKit/AppKit.h>
#include <stdio.h>

@interface HelloDelegate : NSObject {
    NSTextField *_label;
    int _clicks;
}
@end

@implementation HelloDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 120, 440, 260)
                                                   styleMask:NSTitledWindowMask | NSClosableWindowMask |
                                                             NSMiniaturizableWindowMask | NSResizableWindowMask
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setTitle:@"PureDarwin"];
    NSView *content = [window contentView];

    _label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 190, 400, 24)];
    [_label setStringValue:@"Hello from AppKit in a web page"];
    [_label setEditable:NO];
    [_label setBezeled:NO];
    [_label setDrawsBackground:NO];
    [content addSubview:_label];

    NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 130, 400, 24)];
    [field setStringValue:@"Type here"];
    [content addSubview:field];

    NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(20, 40, 160, 32)];
    [button setTitle:@"Click me"];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button setTarget:self];
    [button setAction:@selector(clicked:)];
    [content addSubview:button];

    [window makeKeyAndOrderFront:nil];
    // a headless browser waits for these lines to know where the page is
    printf("PD-READY window %ld\n", (long)[window windowNumber]);
    fflush(stdout);
}

- (void)clicked:(id)sender
{
    _clicks++;
    [_label setStringValue:[NSString stringWithFormat:@"Clicked %d time%s", _clicks, _clicks == 1 ? "" : "s"]];
    printf("PD-CLICK %d\n", _clicks);
    fflush(stdout);
}

@end

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    [app setDelegate:[[HelloDelegate alloc] init]];
    [app run];
    [pool release];
    return 0;
}
