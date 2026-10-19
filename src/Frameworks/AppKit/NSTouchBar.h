// NSTouchBar.h - Touch Bar items, kept as objects on hardware with no Touch Bar
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/NSResponder.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSApplication.h>

@class NSString, NSArray, NSSet, NSDictionary, NSView, NSViewController, NSImage, NSTouchBarItem;
@protocol NSTouchBarDelegate;

typedef NSString *NSTouchBarItemIdentifier;
typedef NSString *NSTouchBarCustomizationIdentifier;
typedef float NSTouchBarItemPriority;

APPKIT_EXPORT NSString *const NSTouchBarItemIdentifierFixedSpaceSmall;
APPKIT_EXPORT NSString *const NSTouchBarItemIdentifierFixedSpaceLarge;
APPKIT_EXPORT NSString *const NSTouchBarItemIdentifierFlexibleSpace;
APPKIT_EXPORT NSString *const NSTouchBarItemIdentifierOtherItemsProxy;

@interface NSTouchBar : NSObject <NSCoding>
@property(copy) NSString *customizationIdentifier;
@property(copy) NSArray *customizationAllowedItemIdentifiers;
@property(copy) NSArray *customizationRequiredItemIdentifiers;
@property(copy) NSArray *defaultItemIdentifiers;
@property(readonly, copy) NSArray *itemIdentifiers;
@property(copy) NSString *principalItemIdentifier;
@property(copy) NSString *escapeKeyReplacementItemIdentifier;
@property(copy) NSSet *templateItems;
@property(assign) id<NSTouchBarDelegate> delegate;
@property(readonly, getter=isVisible) BOOL visible;
@property(class, readonly, getter=isAutomaticCustomizeTouchBarMenuItemEnabled) BOOL automaticCustomizeTouchBarMenuItemEnabled;
- (NSTouchBarItem *)itemForIdentifier:(NSString *)identifier;
@end

@protocol NSTouchBarDelegate <NSObject>
@optional
- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSString *)identifier;
@end

@interface NSTouchBarItem : NSObject <NSCoding>
- (instancetype)initWithIdentifier:(NSString *)identifier;
@property(readonly, copy) NSString *identifier;
@property NSTouchBarItemPriority visibilityPriority;
@property(readonly) NSView *view;
@property(readonly) NSViewController *viewController;
@property(copy) NSString *customizationLabel;
@property(readonly, getter=isVisible) BOOL visible;
@end

@interface NSCustomTouchBarItem : NSTouchBarItem
@property(retain) NSView *view;
@property(retain) NSViewController *viewController;
@end

@interface NSPopoverTouchBarItem : NSTouchBarItem
@property(retain) NSTouchBar *popoverTouchBar;
@property(retain) NSTouchBar *pressAndHoldTouchBar;
@property(copy) NSString *collapsedRepresentationLabel;
@property(retain) NSImage *collapsedRepresentationImage;
@property(retain) NSView *collapsedRepresentation;
@property BOOL showsCloseButton;
- (void)showPopover:(id)sender;
- (void)dismissPopover:(id)sender;
@end

@interface NSSharingServicePickerTouchBarItem : NSTouchBarItem
@property(assign) id delegate;
@property(getter=isEnabled) BOOL enabled;
@property(retain) NSImage *buttonImage;
@end

@interface NSResponder (NSTouchBarProvider)
@property(retain) NSTouchBar *touchBar;
- (NSTouchBar *)makeTouchBar;
@end

@interface NSApplication (NSTouchBarCustomization)
@property(getter=isAutomaticCustomizeTouchBarMenuItemEnabled) BOOL automaticCustomizeTouchBarMenuItemEnabled;
- (void)toggleTouchBarCustomizationPalette:(id)sender;
@end
