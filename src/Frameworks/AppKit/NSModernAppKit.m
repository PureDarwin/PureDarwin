// NSModernAppKit.m - newer API on existing AppKit classes that current applications use
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSModernAppKit.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSMenuView.h>
#import <AppKit/NSNib.h>
#import <AppKit/NSAccessibility.h>
#import <objc/runtime.h>
#import <math.h>

@interface NSSavePanel (PDPrivate)
- (void)_setFilename:(NSString *)filename;
@end

@interface NSMenu (PDPrivate)
+ (void)_pdCancelTracking;
@end

NSString *const NSImageHintCTM = @"ctm";
NSString *const NSImageHintInterpolation = @"interpolation";

static id PDGet(id object, const void *key)
{
    return objc_getAssociatedObject(object, key);
}

static void PDSet(id object, const void *key, id value)
{
    objc_setAssociatedObject(object, key, value, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static NSInteger PDGetInteger(id object, const void *key, NSInteger fallback)
{
    NSNumber *value = PDGet(object, key);

    return value ? [value integerValue] : fallback;
}

static void PDSetInteger(id object, const void *key, NSInteger value)
{
    PDSet(object, key, [NSNumber numberWithInteger:value]);
}

static CGFloat PDLinear(CGFloat c)
{
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static CGFloat PDEncoded(CGFloat c)
{
    c = c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055;
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

@implementation NSColor (ModernColorSpaces)

+ (NSColor *)colorWithSRGBRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha
{
    return [self colorWithCalibratedRed:red green:green blue:blue alpha:alpha];
}

// display p3 shares sRGB's transfer curve, so only the primaries need converting, in linear light
+ (NSColor *)colorWithDisplayP3Red:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha
{
    CGFloat r = PDLinear(red), g = PDLinear(green), b = PDLinear(blue);
    CGFloat sr = 1.2249401 * r - 0.2249404 * g + 0.0000000 * b;
    CGFloat sg = -0.0420569 * r + 1.0420571 * g + 0.0000000 * b;
    CGFloat sb = -0.0196376 * r - 0.0786361 * g + 1.0982735 * b;

    return [self colorWithCalibratedRed:PDEncoded(sr) green:PDEncoded(sg) blue:PDEncoded(sb) alpha:alpha];
}

@end

@implementation NSColorSpace (ModernColorSpaces)

+ (NSColorSpace *)sRGBColorSpace
{
    static NSColorSpace *space;

    if (space == nil) {
        CGColorSpaceRef cg = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        space = [[NSColorSpace alloc] initWithCGColorSpace:cg];
        CGColorSpaceRelease(cg);
    }
    return space;
}

+ (NSColorSpace *)displayP3ColorSpace
{
    static NSColorSpace *space;

    if (space == nil) {
        CGColorSpaceRef cg = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
        space = cg ? [[NSColorSpace alloc] initWithCGColorSpace:cg] : [[self sRGBColorSpace] retain];
        CGColorSpaceRelease(cg);
    }
    return space;
}

@end

@implementation NSGraphicsContext (CGContext)

+ (NSGraphicsContext *)graphicsContextWithCGContext:(CGContextRef)context flipped:(BOOL)flipped
{
    return [self graphicsContextWithGraphicsPort:context flipped:flipped];
}

- (CGContextRef)CGContext
{
    return [self graphicsPort];
}

@end

@implementation NSImage (ModernDrawing)

// drawn once at creation into an offscreen bitmap at the screen's scale, the handler is not kept for later redraws
+ (NSImage *)imageWithSize:(NSSize)size flipped:(BOOL)flipped drawingHandler:(BOOL (^)(NSRect dstRect))drawingHandler
{
    NSImage *image = [[[self alloc] initWithSize:size] autorelease];
    NSRect rect = NSMakeRect(0, 0, size.width, size.height);
    CGFloat scale = [[NSScreen mainScreen] backingScaleFactor];
    NSBitmapImageRep *bitmap;
    NSGraphicsContext *context;

    if (size.width <= 0 || size.height <= 0 || drawingHandler == nil)
        return image;
    if (scale < 1)
        scale = 1;
    bitmap = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:(int)ceil(size.width * scale)
                                                      pixelsHigh:(int)ceil(size.height * scale) bitsPerSample:8 samplesPerPixel:4
                                                        hasAlpha:YES isPlanar:NO colorSpaceName:NSCalibratedRGBColorSpace
                                                     bytesPerRow:0 bitsPerPixel:32] autorelease];
    [bitmap setSize:size];
    context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:context];
    CGContextScaleCTM([context graphicsPort], scale, scale);
    if (flipped) {
        CGContextTranslateCTM([context graphicsPort], 0, size.height);
        CGContextScaleCTM([context graphicsPort], 1, -1);
    }
    drawingHandler(rect);
    [context flushGraphics];
    [NSGraphicsContext restoreGraphicsState];
    [image addRepresentation:bitmap];
    return image;
}

- (CGImageRef)CGImageForProposedRect:(NSRect *)proposedDestRect context:(NSGraphicsContext *)context hints:(NSDictionary *)hints
{
    NSBitmapImageRep *bitmap = nil;
    NSData *tiff;

    for (NSImageRep *rep in [self representations]) {
        if ([rep isKindOfClass:[NSBitmapImageRep class]] && [(NSBitmapImageRep *)rep CGImage] != NULL) {
            bitmap = (NSBitmapImageRep *)rep;
            break;
        }
    }
    // other representations are rendered to a bitmap first
    if (bitmap == nil && (tiff = [self TIFFRepresentation]) != nil)
        bitmap = [[[NSBitmapImageRep alloc] initWithData:tiff] autorelease];
    if (bitmap == nil)
        return NULL;
    if (proposedDestRect != NULL && NSIsEmptyRect(*proposedDestRect))
        *proposedDestRect = NSMakeRect(0, 0, [self size].width, [self size].height);
    return [bitmap CGImage];
}

@end

static char PDSeparatorStyleKey, PDAnimationBehaviorKey;

// carries a sheet's completion block through the delegate-and-selector form
@interface PDSheetHandler : NSObject {
    void (^_handler)(NSModalResponse);
}
@end

@implementation PDSheetHandler

- (instancetype)initWithHandler:(void (^)(NSModalResponse))handler
{
    self = [super init];
    _handler = [handler copy];
    return self;
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(NSInteger)returnCode contextInfo:(void *)info
{
    if (_handler != nil)
        _handler(returnCode);
    [self release];
}

- (void)dealloc
{
    [_handler release];
    [super dealloc];
}

@end

@implementation NSWindow (ModernWindow)

- (NSRect)contentLayoutRect
{
    NSRect rect = [self contentRectForFrameRect:[self frame]];

    rect.origin = NSZeroPoint;
    return rect;
}

- (NSTitlebarSeparatorStyle)titlebarSeparatorStyle
{
    return PDGetInteger(self, &PDSeparatorStyleKey, NSTitlebarSeparatorStyleAutomatic);
}

- (void)setTitlebarSeparatorStyle:(NSTitlebarSeparatorStyle)style
{
    PDSetInteger(self, &PDSeparatorStyleKey, style);
}

- (NSWindowAnimationBehavior)animationBehavior
{
    return PDGetInteger(self, &PDAnimationBehaviorKey, NSWindowAnimationBehaviorDefault);
}

- (void)setAnimationBehavior:(NSWindowAnimationBehavior)behavior
{
    PDSetInteger(self, &PDAnimationBehaviorKey, behavior);
}

- (NSUserInterfaceLayoutDirection)windowTitlebarLayoutDirection
{
    return NSUserInterfaceLayoutDirectionLeftToRight;
}

// windows are never restored from snapshots, so there is nothing to turn off
- (void)disableSnapshotRestoration
{
}

- (void)enableSnapshotRestoration
{
}

- (void)beginSheet:(NSWindow *)sheet completionHandler:(void (^)(NSModalResponse returnCode))handler
{
    PDSheetHandler *target = [[PDSheetHandler alloc] initWithHandler:handler];

    [NSApp beginSheet:sheet modalForWindow:self modalDelegate:target didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}

- (void)beginCriticalSheet:(NSWindow *)sheet completionHandler:(void (^)(NSModalResponse returnCode))handler
{
    [self beginSheet:sheet completionHandler:handler];
}

- (void)endSheet:(NSWindow *)sheet
{
    [NSApp endSheet:sheet returnCode:NSModalResponseStop];
}

- (void)endSheet:(NSWindow *)sheet returnCode:(NSModalResponse)returnCode
{
    [NSApp endSheet:sheet returnCode:returnCode];
}

- (NSWindow *)sheetParent
{
    for (NSWindow *window in [NSApp windows])
        if ([window attachedSheet] == self)
            return window;
    return nil;
}

- (NSArray *)sheets
{
    NSWindow *sheet = [self attachedSheet];

    return sheet != nil ? [NSArray arrayWithObject:sheet] : [NSArray array];
}

@end

static char PDAccessibilityLabelKey, PDLayoutDirectionKey;

@implementation NSView (ModernView)

- (NSEdgeInsets)safeAreaInsets
{
    return NSEdgeInsetsMake(0, 0, 0, 0);
}

- (NSString *)accessibilityLabel
{
    return PDGet(self, &PDAccessibilityLabelKey);
}

- (void)setAccessibilityLabel:(NSString *)label
{
    PDSet(self, &PDAccessibilityLabelKey, [[label copy] autorelease]);
}

- (NSUserInterfaceLayoutDirection)userInterfaceLayoutDirection
{
    return PDGetInteger(self, &PDLayoutDirectionKey, [NSApp userInterfaceLayoutDirection]);
}

- (void)setUserInterfaceLayoutDirection:(NSUserInterfaceLayoutDirection)direction
{
    PDSetInteger(self, &PDLayoutDirectionKey, direction);
}

@end

static NSApplicationPresentationOptions PDPresentationOptions;

@implementation NSApplication (ModernApplication)

- (NSApplicationPresentationOptions)presentationOptions
{
    return PDPresentationOptions;
}

// the options are kept for the app to read back, the shell has no dock or menu bar to hide
- (void)setPresentationOptions:(NSApplicationPresentationOptions)options
{
    PDPresentationOptions = options;
}

- (NSApplicationPresentationOptions)currentSystemPresentationOptions
{
    return PDPresentationOptions;
}

// bit 1 of AppleKeyboardUIMode is the keyboard navigation setting, as on macOS
- (BOOL)isFullKeyboardAccessEnabled
{
    return ([[NSUserDefaults standardUserDefaults] integerForKey:@"AppleKeyboardUIMode"] & 2) != 0;
}

- (NSUserInterfaceLayoutDirection)userInterfaceLayoutDirection
{
    NSString *language = [[[NSLocale preferredLanguages] firstObject] lowercaseString];

    for (NSString *rtl in @[ @"ar", @"he", @"fa", @"ur", @"yi", @"ps", @"ckb" ]) {
        if ([language isEqualToString:rtl] || [language hasPrefix:[rtl stringByAppendingString:@"-"]])
            return NSUserInterfaceLayoutDirectionRightToLeft;
    }
    return NSUserInterfaceLayoutDirectionLeftToRight;
}

- (BOOL)_isRunningModal
{
    return [self modalWindow] != nil;
}

- (BOOL)_isRunningAppModal
{
    return [self modalWindow] != nil;
}

@end

static char PDContextMenuPlugInsKey;

@implementation NSMenu (ModernMenu)

- (BOOL)allowsContextMenuPlugIns
{
    return PDGetInteger(self, &PDContextMenuPlugInsKey, YES);
}

- (void)setAllowsContextMenuPlugIns:(BOOL)allows
{
    PDSetInteger(self, &PDContextMenuPlugInsKey, allows);
}

// ends the tracking loop of whichever menu is open, it returns with no pick
- (void)cancelTracking
{
    [NSMenu _pdCancelTracking];
}

- (void)cancelTrackingWithoutAnimation
{
    [NSMenu _pdCancelTracking];
}

- (void)performActionForItemAtIndex:(NSInteger)index
{
    NSMenuItem *item;

    if (index < 0 || index >= [self numberOfItems])
        return;
    item = [self itemAtIndex:index];
    if (![item isEnabled] || [item action] == NULL)
        return;
    [[NSNotificationCenter defaultCenter] postNotificationName:NSMenuWillSendActionNotification object:self
                                                      userInfo:@{ @"MenuItem" : item }];
    [NSApp sendAction:[item action] to:[item target] from:item];
}

@end

@implementation NSPopUpButtonCell (ModernPopUp)

// the menu opens from trackMouse:, attaching only shows the cell pressed
- (void)attachPopUpWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    [self setHighlighted:YES];
    [controlView setNeedsDisplay:YES];
}

- (void)dismissPopUp
{
    [NSMenu _pdCancelTracking];
    [self setHighlighted:NO];
}

- (void)performClickWithFrame:(NSRect)frame inView:(NSView *)controlView
{
    if ([self trackMouse:[NSApp currentEvent] inRect:frame ofView:controlView untilMouseUp:YES] && [self action] != NULL)
        [NSApp sendAction:[self action] to:[self target] from:controlView];
    [self setHighlighted:NO];
}

@end

static char PDCanSelectHiddenExtensionKey, PDExtensionHiddenKey, PDShowsHiddenFilesKey;

@implementation NSSavePanel (ModernSavePanel)

- (NSString *)nameFieldStringValue
{
    return [[self filename] lastPathComponent] ?: @"";
}

- (void)setNameFieldStringValue:(NSString *)name
{
    [self _setFilename:name];
}

- (NSURL *)directoryURL
{
    return [self directory] ? [NSURL fileURLWithPath:[self directory] isDirectory:YES] : nil;
}

- (void)setDirectoryURL:(NSURL *)url
{
    if ([url isFileURL])
        [self setDirectory:[url path]];
}

- (BOOL)canSelectHiddenExtension
{
    return PDGetInteger(self, &PDCanSelectHiddenExtensionKey, NO);
}

- (void)setCanSelectHiddenExtension:(BOOL)flag
{
    PDSetInteger(self, &PDCanSelectHiddenExtensionKey, flag);
}

- (BOOL)isExtensionHidden
{
    return PDGetInteger(self, &PDExtensionHiddenKey, NO);
}

- (void)setExtensionHidden:(BOOL)flag
{
    PDSetInteger(self, &PDExtensionHiddenKey, flag);
}

- (BOOL)showsHiddenFiles
{
    return PDGetInteger(self, &PDShowsHiddenFilesKey, NO);
}

- (void)setShowsHiddenFiles:(BOOL)flag
{
    PDSetInteger(self, &PDShowsHiddenFilesKey, flag);
}

// the panel runs modally, the handler gets its result on the next turn of the main queue as macOS delivers it
- (void)beginWithCompletionHandler:(void (^)(NSInteger result))handler
{
    NSInteger result = [self runModalForDirectory:[self directory] file:[self nameFieldStringValue]];
    void (^copy)(NSInteger) = [handler copy];

    [self retain];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (copy != nil)
            copy(result);
        [copy release];
        [self release];
    });
}

- (void)beginSheetModalForWindow:(NSWindow *)window completionHandler:(void (^)(NSInteger result))handler
{
    [self beginWithCompletionHandler:handler];
}

@end

static char PDBezelColorKey, PDImageHugsTitleKey;

@implementation NSButton (ModernButton)

+ (instancetype)buttonWithTitle:(NSString *)title target:(id)target action:(SEL)action
{
    NSButton *button = [[[self alloc] initWithFrame:NSMakeRect(0, 0, 80, 32)] autorelease];

    [button setTitle:title];
    [button setTarget:target];
    [button setAction:action];
    [button setButtonType:NSMomentaryPushInButton];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button sizeToFit];
    return button;
}

- (NSColor *)bezelColor
{
    return PDGet(self, &PDBezelColorKey);
}

- (void)setBezelColor:(NSColor *)color
{
    PDSet(self, &PDBezelColorKey, color);
    [self setNeedsDisplay:YES];
}

- (BOOL)imageHugsTitle
{
    return PDGetInteger(self, &PDImageHugsTitleKey, NO);
}

- (void)setImageHugsTitle:(BOOL)flag
{
    PDSetInteger(self, &PDImageHugsTitleKey, flag);
}

@end

@implementation NSTextField (ModernTextField)

+ (instancetype)labelWithString:(NSString *)stringValue
{
    NSTextField *label = [[[self alloc] initWithFrame:NSZeroRect] autorelease];

    [label setStringValue:stringValue ?: @""];
    [label setEditable:NO];
    [label setSelectable:NO];
    [label setBezeled:NO];
    [label setBordered:NO];
    [label setDrawsBackground:NO];
    [label sizeToFit];
    return label;
}

@end

@implementation NSScreen (ModernScreen)

- (NSEdgeInsets)safeAreaInsets
{
    return NSEdgeInsetsMake(0, 0, 0, 0);
}

// screens are standard dynamic range, as maximumExtendedDynamicRangeColorComponentValue says
- (CGFloat)maximumPotentialExtendedDynamicRangeColorComponentValue
{
    return 1.0;
}

@end

static BOOL PDSubstitutionDefault(NSString *key, BOOL fallback)
{
    id value = [[NSUserDefaults standardUserDefaults] objectForKey:key];

    return value ? [value boolValue] : fallback;
}

@implementation NSSpellChecker (ModernSubstitution)

+ (BOOL)isAutomaticDashSubstitutionEnabled
{
    return PDSubstitutionDefault(@"NSAutomaticDashSubstitutionEnabled", YES);
}

+ (BOOL)isAutomaticQuoteSubstitutionEnabled
{
    return PDSubstitutionDefault(@"NSAutomaticQuoteSubstitutionEnabled", YES);
}

+ (BOOL)isAutomaticTextCompletionEnabled
{
    return PDSubstitutionDefault(@"NSAutomaticTextCompletionEnabled", YES);
}

@end

@implementation NSBundle (ModernNibLoading)

- (BOOL)loadNibNamed:(NSString *)nibName owner:(id)owner topLevelObjects:(NSArray **)topLevelObjects
{
    NSNib *nib = [[[NSNib alloc] initWithNibNamed:nibName bundle:self] autorelease];

    if (nib == nil)
        return NO;
    return [nib instantiateNibWithOwner:owner topLevelObjects:topLevelObjects];
}

@end

@implementation NSEvent (ModernPressure)

// events are not numbered by the display, so every event shares number 0
- (NSInteger)eventNumber
{
    return 0;
}

// a mouse without pressure sensing reports full pressure while a button is down, as on macOS
- (CGFloat)pressure
{
    switch ([self type]) {
    case NSLeftMouseDown:
    case NSLeftMouseDragged:
    case NSRightMouseDown:
    case NSRightMouseDragged:
    case NSOtherMouseDown:
    case NSOtherMouseDragged:
        return 1.0f;
    default:
        return 0.0f;
    }
}

@end

@implementation NSScroller (ModernStyle)

// scrollers are always shown, as with a mouse attached on macOS
+ (NSScrollerStyle)preferredScrollerStyle
{
    return NSScrollerStyleLegacy;
}

@end

// the same values the System colour list holds
@implementation NSColor (SystemColors)

+ (NSColor *)systemRedColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemRedColor"];
}

+ (NSColor *)systemGreenColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemGreenColor"];
}

+ (NSColor *)systemBlueColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemBlueColor"];
}

+ (NSColor *)systemOrangeColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemOrangeColor"];
}

+ (NSColor *)systemYellowColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemYellowColor"];
}

+ (NSColor *)systemPurpleColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemPurpleColor"];
}

+ (NSColor *)systemPinkColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemPinkColor"];
}

+ (NSColor *)systemGrayColor
{
    return [self colorWithCatalogName:@"System" colorName:@"systemGrayColor"];
}

@end

@implementation NSCell (FocusRingMask)

// the cell's own drawing is its focus ring shape
- (void)drawFocusRingMaskWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    [self drawWithFrame:cellFrame inView:controlView];
}

@end

static char PDAltersStateKey;

@implementation NSPopUpButtonCell (AltersState)

- (BOOL)altersStateOfSelectedItem
{
    return PDGetInteger(self, &PDAltersStateKey, YES);
}

- (void)setAltersStateOfSelectedItem:(BOOL)flag
{
    PDSetInteger(self, &PDAltersStateKey, flag);
}

@end

static char PDCapInsetsKey, PDResizingModeKey;

@implementation NSImage (Stretching)

- (NSEdgeInsets)capInsets
{
    NSValue *value = PDGet(self, &PDCapInsetsKey);
    NSEdgeInsets insets = NSEdgeInsetsMake(0, 0, 0, 0);

    if (value != nil)
        [value getValue:&insets];
    return insets;
}

- (void)setCapInsets:(NSEdgeInsets)insets
{
    PDSet(self, &PDCapInsetsKey, [NSValue valueWithBytes:&insets objCType:@encode(NSEdgeInsets)]);
}

- (NSImageResizingMode)resizingMode
{
    return PDGetInteger(self, &PDResizingModeKey, NSImageResizingModeTile);
}

- (void)setResizingMode:(NSImageResizingMode)mode
{
    PDSetInteger(self, &PDResizingModeKey, mode);
}

@end

static char PDHuggingKey[2];

@implementation NSView (ContentHugging)

- (NSLayoutPriority)contentHuggingPriorityForOrientation:(NSLayoutConstraintOrientation)orientation
{
    NSNumber *value = PDGet(self, &PDHuggingKey[orientation ? 1 : 0]);

    return value ? [value floatValue] : 250;
}

- (void)setContentHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation
{
    PDSet(self, &PDHuggingKey[orientation ? 1 : 0], [NSNumber numberWithFloat:priority]);
}

@end

@implementation NSObject (AccessibilitySelectors)

- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector
{
    return YES;
}

@end

@implementation NSStatusBarButton

- (instancetype)_initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super initWithFrame:NSMakeRect(0, 0, 22, 22)]) == nil)
        return nil;
    _statusItem = item;
    [self setBordered:NO];
    [self setButtonType:NSMomentaryChangeButton];
    return self;
}

- (BOOL)appearsDisabled
{
    return _appearsDisabled;
}

- (void)setAppearsDisabled:(BOOL)flag
{
    _appearsDisabled = flag;
    [_statusItem setEnabled:!flag];
}

- (void)setImage:(NSImage *)image
{
    [super setImage:image];
    [_statusItem setImage:image];
}

- (void)setAlternateImage:(NSImage *)image
{
    [super setAlternateImage:image];
    [_statusItem setAlternateImage:image];
}

- (void)setTitle:(NSString *)title
{
    [super setTitle:title];
    [_statusItem setTitle:title];
}

- (void)setAttributedTitle:(NSAttributedString *)title
{
    [super setAttributedTitle:title];
    [_statusItem setAttributedTitle:title];
}

- (void)setToolTip:(NSString *)toolTip
{
    [super setToolTip:toolTip];
    [_statusItem setToolTip:toolTip];
}

- (void)setTarget:(id)target
{
    [super setTarget:target];
    [_statusItem setTarget:target];
}

- (void)setAction:(SEL)action
{
    [super setAction:action];
    [_statusItem setAction:action];
}

- (NSInteger)sendActionOn:(NSInteger)mask
{
    return [_statusItem sendActionOn:mask];
}

@end

static char PDStatusButtonKey;

@implementation NSStatusItem (Button)

- (NSStatusBarButton *)button
{
    NSStatusBarButton *button = objc_getAssociatedObject(self, &PDStatusButtonKey);

    if (button == nil) {
        button = [[[NSStatusBarButton alloc] _initWithStatusItem:self] autorelease];
        objc_setAssociatedObject(self, &PDStatusButtonKey, button, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return button;
}

@end

static char PDAlignmentRectKey;

@implementation NSImage (AlignmentRect)

// the whole image unless set, as on macOS
- (NSRect)alignmentRect
{
    NSValue *value = PDGet(self, &PDAlignmentRectKey);

    return value ? [value rectValue] : NSMakeRect(0, 0, [self size].width, [self size].height);
}

- (void)setAlignmentRect:(NSRect)rect
{
    PDSet(self, &PDAlignmentRectKey, [NSValue valueWithRect:rect]);
}

@end

static char PDHideAnchorKey;

@implementation NSPopover (Anchor)

// the popover never draws an arrow here, so the setting is kept for the getter
- (BOOL)shouldHideAnchor
{
    return PDGetInteger(self, &PDHideAnchorKey, NO);
}

- (void)setShouldHideAnchor:(BOOL)flag
{
    PDSetInteger(self, &PDHideAnchorKey, flag);
}

@end

@implementation NSResponder (ParagraphSelection)

// the standard key binding actions go up the responder chain until a text view takes them
- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender
{
    [[self nextResponder] tryToPerform:_cmd with:sender];
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender
{
    [[self nextResponder] tryToPerform:_cmd with:sender];
}

- (void)moveParagraphForwardAndModifySelection:(id)sender
{
    [[self nextResponder] tryToPerform:_cmd with:sender];
}

- (void)moveParagraphBackwardAndModifySelection:(id)sender
{
    [[self nextResponder] tryToPerform:_cmd with:sender];
}

// dictation needs a speech service, which this system does not run
- (void)startDictation:(id)sender
{
    NSBeep();
}

@end
