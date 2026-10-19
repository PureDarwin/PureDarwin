// NSPopover.h - content shown in a panel attached to a rectangle of a view
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSResponder.h>
#import <AppKit/NSAppearance.h>
#import <AppKit/AppKitExport.h>

@class NSViewController, NSView, NSPanel, NSWindow;
@protocol NSPopoverDelegate;

typedef NS_ENUM(NSInteger, NSPopoverBehavior) {
    NSPopoverBehaviorApplicationDefined = 0,
    NSPopoverBehaviorTransient = 1,
    NSPopoverBehaviorSemitransient = 2,
};

APPKIT_EXPORT NSString *const NSPopoverWillShowNotification;
APPKIT_EXPORT NSString *const NSPopoverDidShowNotification;
APPKIT_EXPORT NSString *const NSPopoverWillCloseNotification;
APPKIT_EXPORT NSString *const NSPopoverDidCloseNotification;
APPKIT_EXPORT NSString *const NSPopoverCloseReasonKey;
APPKIT_EXPORT NSString *const NSPopoverCloseReasonStandard;
APPKIT_EXPORT NSString *const NSPopoverCloseReasonDetachToWindow;

@interface NSPopover : NSResponder {
    NSViewController *_contentViewController;
    NSPanel *_panel;
    NSSize _contentSize;
    NSPopoverBehavior _behavior;
    BOOL _animates;
    id _delegate;
    NSAppearance *_appearance;
    NSRect _positioningRect;
}

@property(retain) NSViewController *contentViewController;
@property NSSize contentSize;
@property NSPopoverBehavior behavior;
@property BOOL animates;
@property(assign) id<NSPopoverDelegate> delegate;
@property(retain) NSAppearance *appearance;
@property(readonly) NSAppearance *effectiveAppearance;
@property NSRect positioningRect;
@property(readonly, getter=isShown) BOOL shown;
@property(readonly, getter=isDetached) BOOL detached;

- (void)showRelativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView preferredEdge:(NSRectEdge)preferredEdge;
- (void)performClose:(id)sender;
- (void)close;

@end

@protocol NSPopoverDelegate <NSObject>
@optional
- (BOOL)popoverShouldClose:(NSPopover *)popover;
- (void)popoverWillShow:(NSNotification *)notification;
- (void)popoverDidShow:(NSNotification *)notification;
- (void)popoverWillClose:(NSNotification *)notification;
- (void)popoverDidClose:(NSNotification *)notification;
@end
