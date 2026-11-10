// an app with a menu bar the size of Firefox's: nine menus, many items, submenus and shortcuts
#import <Cocoa/Cocoa.h>

int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];
        const char *titles[] = { "BigMenu", "File", "Edit", "View", "History", "Bookmarks", "Tools", "Window", "Help" };
        for (int m = 0; m < 9; m++) {
            NSMenuItem *top = [[NSMenuItem alloc] initWithTitle:@(titles[m]) action:NULL keyEquivalent:@""];
            NSMenu *menu = [[NSMenu alloc] initWithTitle:@(titles[m])];
            for (int i = 0; i < 40; i++) {
                NSString *title = [NSString stringWithFormat:@"%s item %d with a fairly long title to make the data bigger", titles[m], i];
                NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(terminate:) keyEquivalent:i < 26 ? [NSString stringWithFormat:@"%c", 'a' + i] : @""];
                if (i % 10 == 9) {
                    NSMenu *sub = [[NSMenu alloc] initWithTitle:title];
                    for (int j = 0; j < 20; j++)
                        [sub addItemWithTitle:[NSString stringWithFormat:@"sub %d of %@", j, title] action:NULL keyEquivalent:@""];
                    [item setSubmenu:sub];
                }
                [menu addItem:item];
                if (i % 7 == 6)
                    [menu addItem:[NSMenuItem separatorItem]];
            }
            [top setSubmenu:menu];
            [main addItem:top];
        }
        [NSApp setMainMenu:main];
        for (int w = 0; w < 3; w++) {
            NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100 + 40 * w, 100 + 40 * w, 300, 200)
                styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
            [window setTitle:[NSString stringWithFormat:@"BigMenu %d", w]];
            [window makeKeyAndOrderFront:nil];
        }
        [NSApp activateIgnoringOtherApps:YES];
        [NSTimer scheduledTimerWithTimeInterval:8 repeats:NO block:^(NSTimer *t) { printf("BIGMENU done\n"); exit(0); }];
        [NSApp run];
    }
    return 0;
}
