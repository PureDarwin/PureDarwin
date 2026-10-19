// NSPopover.m - content shown in a panel attached to a rectangle of a view
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSPopover.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSView.h>
#import <AppKit/NSViewController.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSColor.h>
#import <Foundation/Foundation.h>

NSString *const NSPopoverWillShowNotification = @"NSPopoverWillShowNotification";
NSString *const NSPopoverDidShowNotification = @"NSPopoverDidShowNotification";
NSString *const NSPopoverWillCloseNotification = @"NSPopoverWillCloseNotification";
NSString *const NSPopoverDidCloseNotification = @"NSPopoverDidCloseNotification";
NSString *const NSPopoverCloseReasonKey = @"NSPopoverCloseReasonKey";
NSString *const NSPopoverCloseReasonStandard = @"NSPopoverCloseReasonStandard";
NSString *const NSPopoverCloseReasonDetachToWindow = @"NSPopoverCloseReasonDetachToWindow";

// the gap between the anchor rectangle and the popover's edge
static const CGFloat PDPopoverGap = 6;

@implementation NSPopover

@synthesize contentSize = _contentSize, behavior = _behavior, animates = _animates, delegate = _delegate, positioningRect = _positioningRect;

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _animates = YES;
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_panel orderOut:nil];
    [_panel release];
    [_contentViewController release];
    [_appearance release];
    [super dealloc];
}

- (NSViewController *)contentViewController
{
    return _contentViewController;
}

- (void)setContentViewController:(NSViewController *)controller
{
    [controller retain];
    [_contentViewController release];
    _contentViewController = controller;
    if (NSEqualSizes(_contentSize, NSZeroSize))
        _contentSize = [[controller view] frame].size;
}

- (NSAppearance *)appearance
{
    return _appearance;
}

- (void)setAppearance:(NSAppearance *)appearance
{
    [appearance retain];
    [_appearance release];
    _appearance = appearance;
}

- (NSAppearance *)effectiveAppearance
{
    return _appearance ?: [NSApp effectiveAppearance];
}

- (BOOL)isShown
{
    return [_panel isVisible];
}

- (BOOL)isDetached
{
    return NO;
}

- (void)_post:(NSString *)name selector:(SEL)selector
{
    NSNotification *note = [NSNotification notificationWithName:name object:self
        userInfo:@{ NSPopoverCloseReasonKey: NSPopoverCloseReasonStandard }];

    if ([_delegate respondsToSelector:selector])
        [_delegate performSelector:selector withObject:note];
    [[NSNotificationCenter defaultCenter] postNotification:note];
}

// the panel goes on the preferred side of the anchor in screen space, flipped when it would leave the screen
- (void)showRelativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView preferredEdge:(NSRectEdge)preferredEdge
{
    NSView *content = [_contentViewController view];
    NSRect anchor, frame, screen;
    NSSize size = NSEqualSizes(_contentSize, NSZeroSize) ? [content frame].size : _contentSize;

    if (content == nil || [positioningView window] == nil)
        return;
    if (NSIsEmptyRect(positioningRect))
        positioningRect = [positioningView bounds];
    _positioningRect = positioningRect;
    anchor = [[positioningView window] convertRectToScreen:[positioningView convertRect:positioningRect toView:nil]];
    screen = [[[positioningView window] screen] visibleFrame];
    frame.size = size;
    switch (preferredEdge) {
    case NSMinXEdge:
        frame.origin = NSMakePoint(NSMinX(anchor) - size.width - PDPopoverGap, NSMidY(anchor) - size.height / 2);
        break;
    case NSMaxXEdge:
        frame.origin = NSMakePoint(NSMaxX(anchor) + PDPopoverGap, NSMidY(anchor) - size.height / 2);
        break;
    case NSMaxYEdge:
        frame.origin = NSMakePoint(NSMidX(anchor) - size.width / 2, NSMaxY(anchor) + PDPopoverGap);
        if (NSMaxY(frame) > NSMaxY(screen))
            frame.origin.y = NSMinY(anchor) - size.height - PDPopoverGap;
        break;
    default:
        frame.origin = NSMakePoint(NSMidX(anchor) - size.width / 2, NSMinY(anchor) - size.height - PDPopoverGap);
        if (NSMinY(frame) < NSMinY(screen))
            frame.origin.y = NSMaxY(anchor) + PDPopoverGap;
        break;
    }
    frame.origin.x = MAX(NSMinX(screen), MIN(frame.origin.x, NSMaxX(screen) - size.width));

    [self _post:NSPopoverWillShowNotification selector:@selector(popoverWillShow:)];
    if (_panel == nil) {
        _panel = [[NSPanel alloc] initWithContentRect:frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
        [_panel setReleasedWhenClosed:NO];
        [_panel setHasShadow:YES];
        [_panel setBackgroundColor:[NSColor windowBackgroundColor]];
    } else {
        [_panel setFrame:frame display:NO];
    }
    [content setFrame:NSMakeRect(0, 0, size.width, size.height)];
    [_panel setContentView:content];
    [_panel orderFront:nil];
    if (_behavior != NSPopoverBehaviorApplicationDefined)
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_appResigned:)
                                                     name:NSApplicationDidResignActiveNotification object:NSApp];
    [self _post:NSPopoverDidShowNotification selector:@selector(popoverDidShow:)];
}

- (void)_appResigned:(NSNotification *)note
{
    [self performClose:nil];
}

- (void)performClose:(id)sender
{
    if ([_delegate respondsToSelector:@selector(popoverShouldClose:)] && ![_delegate popoverShouldClose:self])
        return;
    [self close];
}

- (void)close
{
    if (![self isShown])
        return;
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidResignActiveNotification object:NSApp];
    [self _post:NSPopoverWillCloseNotification selector:@selector(popoverWillClose:)];
    [_panel orderOut:nil];
    [self _post:NSPopoverDidCloseNotification selector:@selector(popoverDidClose:)];
}

@end
