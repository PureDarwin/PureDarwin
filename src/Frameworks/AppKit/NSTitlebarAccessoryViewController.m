// NSTitlebarAccessoryViewController.m - a view in a window's title bar area, and view controller containment
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSTitlebarAccessoryViewController.h>
#import <AppKit/NSView.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

static char PDParentKey, PDChildrenKey, PDAccessoriesKey;

@implementation NSTitlebarAccessoryViewController

@synthesize layoutAttribute = _layoutAttribute, fullScreenMinHeight = _fullScreenMinHeight, automaticallyAdjustsSize = _automaticallyAdjustsSize;

- (instancetype)initWithNibName:(NSString *)name bundle:(NSBundle *)bundle
{
    if ((self = [super initWithNibName:name bundle:bundle]) == nil)
        return nil;
    _layoutAttribute = NSLayoutAttributeBottom;
    _automaticallyAdjustsSize = YES;
    return self;
}

- (BOOL)isHidden
{
    return _hidden;
}

- (void)setHidden:(BOOL)hidden
{
    _hidden = hidden;
    [[self view] setHidden:hidden];
}

@end

@implementation NSViewController (Containment)

- (NSViewController *)parentViewController
{
    return objc_getAssociatedObject(self, &PDParentKey);
}

- (NSMutableArray *)_children
{
    NSMutableArray *children = objc_getAssociatedObject(self, &PDChildrenKey);

    if (children == nil) {
        children = [NSMutableArray array];
        objc_setAssociatedObject(self, &PDChildrenKey, children, OBJC_ASSOCIATION_RETAIN);
    }
    return children;
}

- (NSArray *)childViewControllers
{
    return [[[self _children] copy] autorelease];
}

- (void)setChildViewControllers:(NSArray *)children
{
    while ([[self _children] count])
        [[[self _children] lastObject] removeFromParentViewController];
    for (NSViewController *child in children)
        [self addChildViewController:child];
}

- (void)insertChildViewController:(NSViewController *)child atIndex:(NSInteger)index
{
    [child removeFromParentViewController];
    [[self _children] insertObject:child atIndex:(NSUInteger)index];
    // the parent holds its children, a child only points back
    objc_setAssociatedObject(child, &PDParentKey, self, OBJC_ASSOCIATION_ASSIGN);
}

- (void)addChildViewController:(NSViewController *)child
{
    [self insertChildViewController:child atIndex:(NSInteger)[[self _children] count]];
}

- (void)removeChildViewControllerAtIndex:(NSInteger)index
{
    NSViewController *child = [[self _children] objectAtIndex:(NSUInteger)index];

    objc_setAssociatedObject(child, &PDParentKey, nil, OBJC_ASSOCIATION_ASSIGN);
    [[self _children] removeObjectAtIndex:(NSUInteger)index];
}

- (void)removeFromParentViewController
{
    NSViewController *parent = [self parentViewController];
    NSUInteger index = [[parent _children] indexOfObjectIdenticalTo:self];

    if (parent != nil && index != NSNotFound)
        [parent removeChildViewControllerAtIndex:(NSInteger)index];
}

@end

@implementation NSWindow (TitlebarAccessories)

- (NSMutableArray *)_accessories
{
    NSMutableArray *accessories = objc_getAssociatedObject(self, &PDAccessoriesKey);

    if (accessories == nil) {
        accessories = [NSMutableArray array];
        objc_setAssociatedObject(self, &PDAccessoriesKey, accessories, OBJC_ASSOCIATION_RETAIN);
    }
    return accessories;
}

- (NSArray *)titlebarAccessoryViewControllers
{
    return [[[self _accessories] copy] autorelease];
}

- (void)setTitlebarAccessoryViewControllers:(NSArray *)controllers
{
    while ([[self _accessories] count])
        [self removeTitlebarAccessoryViewControllerAtIndex:0];
    for (NSTitlebarAccessoryViewController *controller in controllers)
        [self addTitlebarAccessoryViewController:controller];
}

// accessories sit in a strip at the top of the frame view, right-aligned ones against the right edge
- (void)_layoutTitlebarAccessories
{
    NSView *frameView = [[self contentView] superview];
    NSRect bounds = [frameView bounds];
    CGFloat right = NSMaxX(bounds), left = NSMinX(bounds) + 80;

    for (NSTitlebarAccessoryViewController *controller in [self _accessories]) {
        NSView *view = [controller view];
        NSSize size = [view frame].size;
        NSRect frame;

        if ([controller layoutAttribute] == NSLayoutAttributeRight || [controller layoutAttribute] == NSLayoutAttributeTrailing) {
            right -= size.width;
            frame = NSMakeRect(right, NSMaxY(bounds) - size.height, size.width, size.height);
            [view setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
        } else if ([controller layoutAttribute] == NSLayoutAttributeLeft || [controller layoutAttribute] == NSLayoutAttributeLeading) {
            frame = NSMakeRect(left, NSMaxY(bounds) - size.height, size.width, size.height);
            left += size.width;
            [view setAutoresizingMask:NSViewMaxXMargin | NSViewMinYMargin];
        } else {
            frame = NSMakeRect(NSMinX(bounds), NSMaxY(bounds) - size.height, NSWidth(bounds), size.height);
            [view setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
        }
        [view setFrame:frame];
    }
}

- (void)insertTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)controller atIndex:(NSInteger)index
{
    NSView *frameView = [[self contentView] superview];

    [[self _accessories] insertObject:controller atIndex:(NSUInteger)index];
    if (frameView != nil)
        [frameView addSubview:[controller view]];
    [self _layoutTitlebarAccessories];
}

- (void)addTitlebarAccessoryViewController:(NSTitlebarAccessoryViewController *)controller
{
    [self insertTitlebarAccessoryViewController:controller atIndex:(NSInteger)[[self _accessories] count]];
}

- (void)removeTitlebarAccessoryViewControllerAtIndex:(NSInteger)index
{
    NSTitlebarAccessoryViewController *controller = [[self _accessories] objectAtIndex:(NSUInteger)index];

    [[controller view] removeFromSuperview];
    [[self _accessories] removeObjectAtIndex:(NSUInteger)index];
    [self _layoutTitlebarAccessories];
}

@end
