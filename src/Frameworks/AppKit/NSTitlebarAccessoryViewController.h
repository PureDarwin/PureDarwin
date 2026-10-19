// NSTitlebarAccessoryViewController.h - a view in a window's title bar area, and view controller containment
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSViewController.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSLayoutConstraint.h>


@interface NSTitlebarAccessoryViewController : NSViewController {
    NSLayoutAttribute _layoutAttribute;
    CGFloat _fullScreenMinHeight;
    BOOL _hidden;
    BOOL _automaticallyAdjustsSize;
}
@property NSLayoutAttribute layoutAttribute;
@property CGFloat fullScreenMinHeight;
@property(getter=isHidden) BOOL hidden;
@property BOOL automaticallyAdjustsSize;
@end

@interface NSViewController (Containment)
@property(readonly) NSViewController *parentViewController;
@property(copy) NSArray *childViewControllers;
- (void)addChildViewController:(NSViewController *)child;
- (void)insertChildViewController:(NSViewController *)child atIndex:(NSInteger)index;
- (void)removeChildViewControllerAtIndex:(NSInteger)index;
- (void)removeFromParentViewController;
@end

@interface NSWindow (TitlebarAccessories)
@property(copy) NSArray *titlebarAccessoryViewControllers;
- (void)addTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)controller;
- (void)insertTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)controller atIndex:(NSInteger)index;
- (void)removeTitlebarAccessoryViewControllerAtIndex:(NSInteger)index;
@end
