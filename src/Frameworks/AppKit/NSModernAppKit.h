// NSModernAppKit.h - newer API on existing AppKit classes that current applications use
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSColorSpace.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSPopUpButtonCell.h>
#import <AppKit/NSSavePanel.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSSpellChecker.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSScroller.h>
#import <AppKit/NSCell.h>
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSStatusItem.h>
#import <AppKit/NSPopover.h>

#ifndef PD_NSEDGEINSETS_DEFINED
#define PD_NSEDGEINSETS_DEFINED 1
typedef struct NSEdgeInsets {
    CGFloat top;
    CGFloat left;
    CGFloat bottom;
    CGFloat right;
} NSEdgeInsets;

static inline NSEdgeInsets NSEdgeInsetsMake(CGFloat top, CGFloat left, CGFloat bottom, CGFloat right)
{
    NSEdgeInsets insets = { top, left, bottom, right };
    return insets;
}
#endif

typedef NS_ENUM(NSInteger, NSUserInterfaceLayoutDirection) {
    NSUserInterfaceLayoutDirectionLeftToRight = 0,
    NSUserInterfaceLayoutDirectionRightToLeft = 1,
};

typedef NS_OPTIONS(NSUInteger, NSApplicationPresentationOptions) {
    NSApplicationPresentationDefault = 0,
    NSApplicationPresentationAutoHideDock = 1 << 0,
    NSApplicationPresentationHideDock = 1 << 1,
    NSApplicationPresentationAutoHideMenuBar = 1 << 2,
    NSApplicationPresentationHideMenuBar = 1 << 3,
    NSApplicationPresentationDisableAppleMenu = 1 << 4,
    NSApplicationPresentationDisableProcessSwitching = 1 << 5,
    NSApplicationPresentationDisableForceQuit = 1 << 6,
    NSApplicationPresentationDisableSessionTermination = 1 << 7,
    NSApplicationPresentationDisableHideApplication = 1 << 8,
    NSApplicationPresentationDisableMenuBarTransparency = 1 << 9,
    NSApplicationPresentationFullScreen = 1 << 10,
    NSApplicationPresentationAutoHideToolbar = 1 << 11,
    NSApplicationPresentationDisableCursorLocationAssistance = 1 << 12,
};

typedef NS_ENUM(NSInteger, NSWindowAnimationBehavior) {
    NSWindowAnimationBehaviorDefault = 0,
    NSWindowAnimationBehaviorNone = 2,
    NSWindowAnimationBehaviorDocumentWindow = 3,
    NSWindowAnimationBehaviorUtilityWindow = 4,
    NSWindowAnimationBehaviorAlertPanel = 5,
};

typedef NS_ENUM(NSInteger, NSTitlebarSeparatorStyle) {
    NSTitlebarSeparatorStyleAutomatic = 0,
    NSTitlebarSeparatorStyleNone,
    NSTitlebarSeparatorStyleLine,
    NSTitlebarSeparatorStyleShadow,
};

APPKIT_EXPORT NSString *const NSImageHintCTM;
APPKIT_EXPORT NSString *const NSImageHintInterpolation;

@interface NSColor (ModernColorSpaces)
+ (NSColor *)colorWithSRGBRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithDisplayP3Red:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
@end

@interface NSColorSpace (ModernColorSpaces)
+ (NSColorSpace *)sRGBColorSpace;
+ (NSColorSpace *)displayP3ColorSpace;
@end

@interface NSGraphicsContext (CGContext)
+ (NSGraphicsContext *)graphicsContextWithCGContext:(CGContextRef)context flipped:(BOOL)flipped;
@property(readonly) CGContextRef CGContext;
@end

@interface NSImage (ModernDrawing)
+ (NSImage *)imageWithSize:(NSSize)size flipped:(BOOL)flipped drawingHandler:(BOOL (^)(NSRect dstRect))drawingHandler;
- (CGImageRef)CGImageForProposedRect:(NSRect *)proposedDestRect context:(NSGraphicsContext *)context hints:(NSDictionary *)hints;
@end

@interface NSWindow (ModernWindow)
@property(readonly) NSRect contentLayoutRect;
@property NSTitlebarSeparatorStyle titlebarSeparatorStyle;
@property NSWindowAnimationBehavior animationBehavior;
@property(readonly) NSUserInterfaceLayoutDirection windowTitlebarLayoutDirection;
- (void)disableSnapshotRestoration;
- (void)enableSnapshotRestoration;
- (void)beginSheet:(NSWindow *)sheet completionHandler:(void (^)(NSModalResponse returnCode))handler;
- (void)beginCriticalSheet:(NSWindow *)sheet completionHandler:(void (^)(NSModalResponse returnCode))handler;
- (void)endSheet:(NSWindow *)sheet;
- (void)endSheet:(NSWindow *)sheet returnCode:(NSModalResponse)returnCode;
@property(readonly) NSWindow *sheetParent;
@property(readonly) NSArray *sheets;
@end

@interface NSView (ModernView)
@property(readonly) NSEdgeInsets safeAreaInsets;
@property(copy) NSString *accessibilityLabel;
@property NSUserInterfaceLayoutDirection userInterfaceLayoutDirection;
@end

@interface NSApplication (ModernApplication)
@property NSApplicationPresentationOptions presentationOptions;
@property(readonly) NSApplicationPresentationOptions currentSystemPresentationOptions;
@property(readonly, getter=isFullKeyboardAccessEnabled) BOOL fullKeyboardAccessEnabled;
@property(readonly) NSUserInterfaceLayoutDirection userInterfaceLayoutDirection;
- (BOOL)_isRunningModal;
- (BOOL)_isRunningAppModal;
@end

@interface NSMenu (ModernMenu)
@property BOOL allowsContextMenuPlugIns;
- (void)cancelTracking;
- (void)cancelTrackingWithoutAnimation;
- (void)performActionForItemAtIndex:(NSInteger)index;
@end

@interface NSPopUpButtonCell (ModernPopUp)
- (void)attachPopUpWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
- (void)dismissPopUp;
- (void)performClickWithFrame:(NSRect)frame inView:(NSView *)controlView;
@end

@interface NSSavePanel (ModernSavePanel)
@property(copy) NSString *nameFieldStringValue;
@property(copy) NSURL *directoryURL;
@property BOOL canSelectHiddenExtension;
@property(getter=isExtensionHidden) BOOL extensionHidden;
@property BOOL showsHiddenFiles;
- (void)beginWithCompletionHandler:(void (^)(NSInteger result))handler;
- (void)beginSheetModalForWindow:(NSWindow *)window completionHandler:(void (^)(NSInteger result))handler;
@end

@interface NSButton (ModernButton)
+ (instancetype)buttonWithTitle:(NSString *)title target:(id)target action:(SEL)action;
@property(copy) NSColor *bezelColor;
@property BOOL imageHugsTitle;
@end

@interface NSTextField (ModernTextField)
+ (instancetype)labelWithString:(NSString *)stringValue;
@end

@interface NSScreen (ModernScreen)
@property(readonly) NSEdgeInsets safeAreaInsets;
@property(readonly) CGFloat maximumPotentialExtendedDynamicRangeColorComponentValue;
@end

@interface NSSpellChecker (ModernSubstitution)
+ (BOOL)isAutomaticDashSubstitutionEnabled;
+ (BOOL)isAutomaticQuoteSubstitutionEnabled;
+ (BOOL)isAutomaticTextCompletionEnabled;
@end

@interface NSBundle (ModernNibLoading)
- (BOOL)loadNibNamed:(NSString *)nibName owner:(id)owner topLevelObjects:(NSArray **)topLevelObjects;
@end

typedef NS_ENUM(NSInteger, NSScrollerStyle) {
    NSScrollerStyleLegacy = 0,
    NSScrollerStyleOverlay,
};

typedef NS_ENUM(NSInteger, NSImageResizingMode) {
    NSImageResizingModeTile = 0,
    NSImageResizingModeStretch,
};

typedef NS_ENUM(NSInteger, NSLayoutConstraintOrientation) {
    NSLayoutConstraintOrientationHorizontal = 0,
    NSLayoutConstraintOrientationVertical = 1,
};

@interface NSEvent (ModernPressure)
@property(readonly) NSInteger eventNumber;
@property(readonly) CGFloat pressure;
@end

@interface NSScroller (ModernStyle)
+ (NSScrollerStyle)preferredScrollerStyle;
@end

@interface NSColor (SystemColors)
+ (NSColor *)systemRedColor;
+ (NSColor *)systemGreenColor;
+ (NSColor *)systemBlueColor;
+ (NSColor *)systemOrangeColor;
+ (NSColor *)systemYellowColor;
+ (NSColor *)systemPurpleColor;
+ (NSColor *)systemPinkColor;
+ (NSColor *)systemGrayColor;
@end

@interface NSCell (FocusRingMask)
- (void)drawFocusRingMaskWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
@end

@interface NSPopUpButtonCell (AltersState)
@property BOOL altersStateOfSelectedItem;
@end

@interface NSImage (Stretching)
@property NSEdgeInsets capInsets;
@property NSImageResizingMode resizingMode;
@end

@interface NSView (ContentHugging)
- (NSLayoutPriority)contentHuggingPriorityForOrientation:(NSLayoutConstraintOrientation)orientation;
- (void)setContentHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation;
@end

@interface NSObject (AccessibilitySelectors)
- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector;
@end

@class NSStatusItem;

// the button a status item shows, its image, title and action reach the item
@interface NSStatusBarButton : NSButton {
    NSStatusItem *_statusItem;
    BOOL _appearsDisabled;
}
@property BOOL appearsDisabled;
@end

@interface NSStatusItem (Button)
@property(readonly, strong) NSStatusBarButton *button;
@end

@interface NSImage (AlignmentRect)
@property NSRect alignmentRect;
@end

@interface NSPopover (Anchor)
@property BOOL shouldHideAnchor;
@end

@interface NSResponder (ParagraphSelection)
- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender;
- (void)moveToEndOfParagraphAndModifySelection:(id)sender;
- (void)moveParagraphForwardAndModifySelection:(id)sender;
- (void)moveParagraphBackwardAndModifySelection:(id)sender;
- (void)startDictation:(id)sender;
@end
