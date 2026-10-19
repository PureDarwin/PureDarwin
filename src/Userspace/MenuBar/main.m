/*
 * main.m - MenuBar: the system menu bar, showing the menus of the app in front.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "PDMenuServer.h"
#import "MenuBarView.h"
#include <dlfcn.h>
#include <stdint.h>
#if __has_include(<notify.h>)
#include <notify.h>
#else
// libSystem exports notify(3) but this sdk does not ship its header
#define NOTIFY_STATUS_OK 0
uint32_t notify_post(const char *name);
uint32_t notify_register_check(const char *name, int *out_token);
uint32_t notify_register_file_descriptor(const char *name, int *notify_fd, int flags, int *out_token);
uint32_t notify_set_state(int token, uint64_t state64);
uint32_t notify_get_state(int token, uint64_t *state64);
uint32_t notify_cancel(int token);
#endif
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ucontext.h>
#include <unistd.h>

@interface NSMenu (PDCancelTracking)
+ (void)_pdCancelTracking;
@end

@interface NSWindow (PDBlur)
- (void)_setBackgroundBlurInset:(CGFloat)inset radius:(CGFloat)radius;
@end

// the server keeps each client's published menu and shows the one whose window has focus
@interface MenuBarServer : NSObject <PDMenuServer, MenuBarViewDelegate> {
    NSConnection *_connection;
    NSMutableDictionary *_menus;
    NSMutableDictionary *_windows;
    NSString *_active;
    NSNumber *_activeWindow;
    NSWindow *_window;
    MenuBarView *_view;
    NSTimer *_clock;
}
@end

@implementation MenuBarServer

// spans the top of the main screen, retried until the compositor has reported its size
- (void)placeBar:(NSTimer *)timer
{
    NSRect screen = [[NSScreen mainScreen] frame];

    if (NSWidth(screen) < 100) {
        [NSTimer scheduledTimerWithTimeInterval:0.25 target:self selector:@selector(placeBar:) userInfo:nil repeats:NO];
        return;
    }
    NSLog(@"MenuBar: screen %@", NSStringFromRect(screen));
    [_window setFrame:NSMakeRect(NSMinX(screen), NSMaxY(screen) - MenuBarHeight, NSWidth(screen), MenuBarHeight) display:YES];
    [_window orderFront:nil];
    [_window _setBackgroundBlurInset:0 radius:0];
}

// the compositor starts with rounded, shadowed windows, a login applies the saved choice
static void pushWindowStyle(void)
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id round = [defaults objectForKey:@"PDWindowRoundedCorners"], shadow = [defaults objectForKey:@"PDWindowShadows"];
    uint64_t style = (round && ![round boolValue] ? 1 : 0) | (shadow && ![shadow boolValue] ? 2 : 0);
    int token;

    if (style == 0 || notify_register_check("org.puredarwin.WindowServer.windowstyle", &token) != NOTIFY_STATUS_OK)
        return;
    notify_set_state(token, style);
    notify_post("org.puredarwin.WindowServer.windowstyle");
    notify_cancel(token);
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect frame = NSMakeRect(0, 0, 1024, MenuBarHeight);

    pushWindowStyle();

    // a display mode change moves the screen edges
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(placeBar:)
                                                 name:NSApplicationDidChangeScreenParametersNotification object:nil];
    _menus = [[NSMutableDictionary alloc] init];
    _windows = [[NSMutableDictionary alloc] init];

    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setLevel:NSMainMenuWindowLevel];
    // translucent over a blurred desktop, as in the demo
    [_window setOpaque:NO];
    [_window setBackgroundColor:[NSColor clearColor]];
    _view = [[MenuBarView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(frame), MenuBarHeight)];
    [_view setDelegate:self];
    [_view setAutoresizingMask:NSViewWidthSizable];
    [_window setContentView:_view];
    [self placeBar:nil];

    _connection = [[NSConnection alloc] init];
    [_connection setRootObject:self];
    if (![_connection registerName:PDMenuServerName])
        NSLog(@"MenuBar: could not register %@", PDMenuServerName);
    else
        NSLog(@"MenuBar: serving %@", PDMenuServerName);

    _clock = [[NSTimer scheduledTimerWithTimeInterval:10 target:_view selector:@selector(tick:)
                                             userInfo:nil repeats:YES] retain];
}

- (void)showActive
{
    [_view setMenuData:_active ? [_menus objectForKey:_active] : nil];
}

- (oneway void)updateMenuForWindow:(bycopy NSNumber *)windowId
                          menuData:(bycopy NSDictionary *)menuData
                        clientName:(bycopy NSString *)clientName
{
    if (clientName == nil || menuData == nil)
        return;
    NSLog(@"MenuBar: %@ published %lu menus", clientName, (unsigned long)[[menuData objectForKey:@"items"] count]);
    [_menus setObject:menuData forKey:clientName];
    [_windows setObject:clientName forKey:windowId];

    // the first app to publish owns the bar until something takes focus
    if (_active == nil) {
        _active = [clientName copy];
        _activeWindow = [windowId retain];
    }
    if ([_active isEqualToString:clientName])
        [self showActive];
}

- (oneway void)activateClient:(bycopy NSString *)clientName forWindow:(bycopy NSNumber *)windowId
{
    // a click in another app's window ends any open menu, as on macOS
    [NSMenu _pdCancelTracking];
    if (clientName == nil || [_active isEqualToString:clientName])
        return;
    [_active release];
    [_activeWindow release];
    _active = [clientName copy];
    _activeWindow = [windowId retain];
    [self showActive];
}

// the clicked item is sent back to its app, which runs the action itself
- (void)menuBarView:(MenuBarView *)view pickedPath:(NSArray *)path
{
    id client;

    if (_active == nil)
        return;
    NS_DURING
        client = [NSConnection rootProxyForConnectionWithRegisteredName:_active host:nil];
        [client setProtocolForProxy:@protocol(PDMenuClient)];
        [(id<PDMenuClient>)client activateMenuItemAtPath:path forWindow:_activeWindow];
    NS_HANDLER
        // the app has gone, so drop its menus
        [_menus removeObjectForKey:_active];
        [_active release];
        _active = nil;
        [self showActive];
    NS_ENDHANDLER
}

@end

// a crash prints where it happened, the kernel alone only reports the signal
static void crashed(int sig, siginfo_t *info, void *context)
{
    void *frames[48];
    int count = backtrace(frames, 48);

    ucontext_t *uc = context;
#if defined(__x86_64__)
    void *pc = (void *)uc->uc_mcontext->__ss.__rip;
#else
    // arm64 names them pc and fp
    void *pc = (void *)uc->uc_mcontext->__ss.__pc;
#endif
    Dl_info at;

    fprintf(stderr, "MenuBar: signal %d at %p\n", sig, info->si_addr);
    // the faulting instruction first, the backtrace below may not survive a corrupted stack
    if (dladdr(pc, &at) && at.dli_sname != NULL)
        fprintf(stderr, "  pc %s + %ld (%s)\n", at.dli_sname, (long)((char *)pc - (char *)at.dli_saddr), at.dli_fname);
    else
        fprintf(stderr, "  pc %p\n", pc);
    // callers from the saved frame pointers
#if defined(__x86_64__)
    void **fp = (void **)uc->uc_mcontext->__ss.__rbp;
#else
    void **fp = (void **)uc->uc_mcontext->__ss.__fp;
#endif

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
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    MenuBarServer *server;

    [NSApplication sharedApplication];
    server = [[MenuBarServer alloc] init];
    [NSApp setDelegate:server];
    [NSApp run];
    [pool release];
    return 0;
}
