// NSTouchBar.m - Touch Bar items, kept as objects on hardware with no Touch Bar
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSTouchBar.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSView.h>
#import <AppKit/NSViewController.h>
#import <AppKit/NSImage.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

NSString *const NSTouchBarItemIdentifierFixedSpaceSmall = @"NSTouchBarItemIdentifierFixedSpaceSmall";
NSString *const NSTouchBarItemIdentifierFixedSpaceLarge = @"NSTouchBarItemIdentifierFixedSpaceLarge";
NSString *const NSTouchBarItemIdentifierFlexibleSpace = @"NSTouchBarItemIdentifierFlexibleSpace";
NSString *const NSTouchBarItemIdentifierOtherItemsProxy = @"NSTouchBarItemIdentifierOtherItemsProxy";

// there is no Touch Bar to show items on, so a bar is a list of items nothing displays
@implementation NSTouchBar {
    NSMutableDictionary *_items;
}

@synthesize customizationIdentifier = _customizationIdentifier, customizationAllowedItemIdentifiers = _allowed;
@synthesize customizationRequiredItemIdentifiers = _required, defaultItemIdentifiers = _defaults;
@synthesize principalItemIdentifier = _principal, escapeKeyReplacementItemIdentifier = _escape;
@synthesize templateItems = _templateItems, delegate = _delegate;

+ (BOOL)isAutomaticCustomizeTouchBarMenuItemEnabled
{
    return NO;
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _items = [[NSMutableDictionary alloc] init];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    return [self init];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

- (void)dealloc
{
    [_items release];
    [_customizationIdentifier release];
    [_allowed release];
    [_required release];
    [_defaults release];
    [_principal release];
    [_escape release];
    [_templateItems release];
    [super dealloc];
}

- (NSArray *)itemIdentifiers
{
    return _defaults ?: @[];
}

- (BOOL)isVisible
{
    return NO;
}

// a template item first, then whatever the delegate makes, kept for the next ask
- (NSTouchBarItem *)itemForIdentifier:(NSString *)identifier
{
    NSTouchBarItem *item = [_items objectForKey:identifier];

    if (item != nil || identifier == nil)
        return item;
    for (NSTouchBarItem *candidate in _templateItems) {
        if ([[candidate identifier] isEqualToString:identifier])
            item = candidate;
    }
    if (item == nil && [_delegate respondsToSelector:@selector(touchBar:makeItemForIdentifier:)])
        item = [_delegate touchBar:self makeItemForIdentifier:identifier];
    if (item != nil)
        [_items setObject:item forKey:identifier];
    return item;
}

@end

@implementation NSTouchBarItem

@synthesize identifier = _identifier, visibilityPriority = _visibilityPriority, customizationLabel = _customizationLabel;

- (instancetype)initWithIdentifier:(NSString *)identifier
{
    if ((self = [super init]) == nil)
        return nil;
    _identifier = [identifier copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    return [self initWithIdentifier:nil];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

- (void)dealloc
{
    [_identifier release];
    [_customizationLabel release];
    [super dealloc];
}

- (NSView *)view
{
    return nil;
}

- (NSViewController *)viewController
{
    return nil;
}

- (BOOL)isVisible
{
    return NO;
}

@end

@implementation NSCustomTouchBarItem

@synthesize view = _view, viewController = _viewController;

- (void)dealloc
{
    [_view release];
    [_viewController release];
    [super dealloc];
}

- (NSView *)view
{
    return _view ?: [_viewController view];
}

@end

@implementation NSPopoverTouchBarItem

@synthesize popoverTouchBar = _popoverTouchBar, pressAndHoldTouchBar = _pressAndHoldTouchBar;
@synthesize collapsedRepresentationLabel = _label, collapsedRepresentationImage = _image;
@synthesize collapsedRepresentation = _collapsedRepresentation, showsCloseButton = _showsCloseButton;

- (void)dealloc
{
    [_popoverTouchBar release];
    [_pressAndHoldTouchBar release];
    [_label release];
    [_image release];
    [_collapsedRepresentation release];
    [super dealloc];
}

- (void)showPopover:(id)sender
{
}

- (void)dismissPopover:(id)sender
{
}

@end

@implementation NSSharingServicePickerTouchBarItem

@synthesize delegate = _delegate, enabled = _enabled, buttonImage = _buttonImage;

- (void)dealloc
{
    [_buttonImage release];
    [super dealloc];
}

@end

static char PDTouchBarKey;

@implementation NSResponder (NSTouchBarProvider)

- (NSTouchBar *)touchBar
{
    NSTouchBar *bar = objc_getAssociatedObject(self, &PDTouchBarKey);

    if (bar == nil && (bar = [self makeTouchBar]) != nil)
        objc_setAssociatedObject(self, &PDTouchBarKey, bar, OBJC_ASSOCIATION_RETAIN);
    return bar;
}

- (void)setTouchBar:(NSTouchBar *)touchBar
{
    objc_setAssociatedObject(self, &PDTouchBarKey, touchBar, OBJC_ASSOCIATION_RETAIN);
}

- (NSTouchBar *)makeTouchBar
{
    return nil;
}

@end

@implementation NSApplication (NSTouchBarCustomization)

- (BOOL)isAutomaticCustomizeTouchBarMenuItemEnabled
{
    return NO;
}

- (void)setAutomaticCustomizeTouchBarMenuItemEnabled:(BOOL)enabled
{
}

- (void)toggleTouchBarCustomizationPalette:(id)sender
{
}

@end
