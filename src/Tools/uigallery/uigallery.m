// uigallery.m - draws a fixed set of AppKit controls offscreen to PNGs, for comparing PureDarwin with macOS
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// usage: uigallery OUTDIR
// views.png: the controls as views in a window's content view, rendered with cacheDisplayInRect
// cells-flipped.png and cells-unflipped.png: each control's cell drawn with drawWithFrame:inView: into a
// CGBitmapContext, the way Qt's macOS style paints its widgets
// extras-light.png and extras-dark.png: more states and control sizes, drawn the same way into a flipped bitmap

#import <AppKit/AppKit.h>

#define W 640
#define H 760

static NSButton *button(NSString *title, NSButtonType type, NSBezelStyle bezel, NSControlStateValue state, BOOL enabled)
{
    NSButton *b = [[NSButton alloc] initWithFrame:NSZeroRect];

    [b setButtonType:type];
    if (type == NSButtonTypeMomentaryPushIn)
        [b setBezelStyle:bezel];
    [b setTitle:title];
    [b setState:state];
    [b setEnabled:enabled];
    if (type == NSButtonTypeSwitch || type == NSButtonTypeRadio)
        [b setAllowsMixedState:YES];
    return b;
}

static NSProgressIndicator *progress(double value, BOOL indeterminate, NSProgressIndicatorStyle style)
{
    NSProgressIndicator *p = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];

    [p setStyle:style];
    [p setIndeterminate:indeterminate];
    [p setMinValue:0];
    [p setMaxValue:100];
    [p setDoubleValue:value];
    return p;
}

// every row is a label and a control, laid out top to bottom at fixed positions
static NSArray *controls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"push", button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), 100, 24);
    add(@"push default", ({ NSButton *b = button(@"Launch", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES); [b setKeyEquivalent:@"\r"]; b; }), 100, 24);
    add(@"push disabled", button(@"Kill", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, NO), 100, 24);
    add(@"push regular square", button(@"Find", NSButtonTypeMomentaryPushIn, NSBezelStyleRegularSquare, NSControlStateValueOff, YES), 100, 24);
    add(@"check off", button(@"Keep updating", NSButtonTypeSwitch, 0, NSControlStateValueOff, YES), 160, 18);
    add(@"check on", button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), 160, 18);
    add(@"check mixed", button(@"Color lines", NSButtonTypeSwitch, 0, NSControlStateValueMixed, YES), 160, 18);
    add(@"check on disabled", button(@"Disabled", NSButtonTypeSwitch, 0, NSControlStateValueOn, NO), 160, 18);
    add(@"radio off", button(@"Option A", NSButtonTypeRadio, 0, NSControlStateValueOff, YES), 160, 18);
    add(@"radio on", button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, YES), 160, 18);
    add(@"text field", ({ NSTextField *t = [[NSTextField alloc] initWithFrame:NSZeroRect]; [t setStringValue:@"FaintLove"]; t; }), 200, 22);
    add(@"text placeholder", ({ NSTextField *t = [[NSTextField alloc] initWithFrame:NSZeroRect]; [t setPlaceholderString:@"Search"]; t; }), 200, 22);
    add(@"search field", ({ NSSearchField *s = [[NSSearchField alloc] initWithFrame:NSZeroRect]; [s setPlaceholderString:@"Search"]; s; }), 200, 22);
    add(@"label", [NSTextField labelWithString:@"Minecraft Log"], 200, 17);
    add(@"popup", ({ NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]; [p addItemsWithTitles:@[ @"Vulkan", @"OpenGL" ]]; p; }), 140, 24);
    add(@"combo", ({ NSComboBox *c = [[NSComboBox alloc] initWithFrame:NSZeroRect]; [c addItemsWithObjectValues:@[ @"26.3", @"26.2" ]]; [c setStringValue:@"26.3"]; c; }), 140, 24);
    add(@"progress 40%", progress(40, NO, NSProgressIndicatorStyleBar), 240, 20);
    add(@"progress 100%", progress(100, NO, NSProgressIndicatorStyleBar), 240, 20);
    add(@"progress indeterminate", progress(0, YES, NSProgressIndicatorStyleBar), 240, 20);
    add(@"slider", ({ NSSlider *s = [NSSlider sliderWithValue:30 minValue:0 maxValue:100 target:nil action:NULL]; s; }), 200, 22);
    add(@"stepper", ({ NSStepper *s = [[NSStepper alloc] initWithFrame:NSZeroRect]; [s setIntegerValue:3]; s; }), 19, 27);
    add(@"segmented", ({ NSSegmentedControl *s = [NSSegmentedControl segmentedControlWithLabels:@[ @"Log", @"Version", @"Mods" ] trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:NULL]; [s setSelectedSegment:0]; s; }), 200, 24);
    add(@"box", ({ NSBox *b = [[NSBox alloc] initWithFrame:NSZeroRect]; [b setTitle:@"Group"]; b; }), 200, 40);
    return rows;
}

static NSButton *sized(NSButton *b, NSControlSize size)
{
    [b setControlSize:size];
    [b setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    return b;
}

// states and sizes the main gallery leaves out
static NSArray *extraControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"push small", sized(button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), NSControlSizeSmall), 100, 21);
    add(@"push mini", sized(button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), NSControlSizeMini), 100, 16);
    add(@"push pressed", ({ NSButton *b = button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES); [[b cell] setHighlighted:YES]; b; }), 100, 24);
    add(@"push frame 32", button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), 100, 32);
    add(@"check small on", sized(button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), NSControlSizeSmall), 160, 18);
    add(@"check mini on", sized(button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), NSControlSizeMini), 160, 18);
    add(@"check off disabled", button(@"Disabled", NSButtonTypeSwitch, 0, NSControlStateValueOff, NO), 160, 18);
    add(@"check pressed", ({ NSButton *b = button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES); [[b cell] setHighlighted:YES]; b; }), 160, 18);
    add(@"radio small on", sized(button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, YES), NSControlSizeSmall), 160, 18);
    add(@"radio on disabled", button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, NO), 160, 18);
    add(@"search with text", ({ NSSearchField *s = [[NSSearchField alloc] initWithFrame:NSZeroRect]; [s setStringValue:@"fabric"]; s; }), 200, 22);
    add(@"popup disabled", ({ NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]; [p addItemsWithTitles:@[ @"Vulkan" ]]; [p setEnabled:NO]; p; }), 140, 24);
    add(@"slider 70% h28", ({ NSSlider *s = [NSSlider sliderWithValue:70 minValue:0 maxValue:100 target:nil action:NULL]; s; }), 200, 28);
    add(@"segmented second", ({ NSSegmentedControl *s = [NSSegmentedControl segmentedControlWithLabels:@[ @"Log", @"Version", @"Mods" ] trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:NULL]; [s setSelectedSegment:1]; s; }), 200, 24);
    return rows;
}

static NSButton *sized(NSButton *b, NSControlSize size)
{
    [b setControlSize:size];
    [b setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    return b;
}

// states and sizes the main gallery leaves out
static NSArray *extraControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"push small", sized(button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), NSControlSizeSmall), 100, 21);
    add(@"push mini", sized(button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), NSControlSizeMini), 100, 16);
    add(@"push pressed", ({ NSButton *b = button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES); [[b cell] setHighlighted:YES]; b; }), 100, 24);
    add(@"push frame 32", button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), 100, 32);
    add(@"check small on", sized(button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), NSControlSizeSmall), 160, 18);
    add(@"check mini on", sized(button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), NSControlSizeMini), 160, 18);
    add(@"check off disabled", button(@"Disabled", NSButtonTypeSwitch, 0, NSControlStateValueOff, NO), 160, 18);
    add(@"check pressed", ({ NSButton *b = button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES); [[b cell] setHighlighted:YES]; b; }), 160, 18);
    add(@"radio small on", sized(button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, YES), NSControlSizeSmall), 160, 18);
    add(@"radio on disabled", button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, NO), 160, 18);
    add(@"search with text", ({ NSSearchField *s = [[NSSearchField alloc] initWithFrame:NSZeroRect]; [s setStringValue:@"fabric"]; s; }), 200, 22);
    add(@"popup disabled", ({ NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]; [p addItemsWithTitles:@[ @"Vulkan" ]]; [p setEnabled:NO]; p; }), 140, 24);
    add(@"slider 70% h28", ({ NSSlider *s = [NSSlider sliderWithValue:70 minValue:0 maxValue:100 target:nil action:NULL]; s; }), 200, 28);
    add(@"segmented second", ({ NSSegmentedControl *s = [NSSegmentedControl segmentedControlWithLabels:@[ @"Log", @"Version", @"Mods" ] trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:NULL]; [s setSelectedSegment:1]; s; }), 200, 24);
    return rows;
}

static NSButton *bezel(NSString *title, NSBezelStyle style, BOOL enabled)
{
    NSButton *b = button(title, NSButtonTypeMomentaryPushIn, style, NSControlStateValueOff, enabled);

    [b setBezelStyle:style];
    return b;
}

// the bezel styles Qt's macOS style draws with, beyond the rounded push button
static NSArray *bezelControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"textured square", bezel(@"Launch", NSBezelStyleTexturedSquare, YES), 100, 24);
    add(@"textured square off", bezel(@"Kill", NSBezelStyleTexturedSquare, NO), 100, 24);
    add(@"small square", bezel(@"Edit", NSBezelStyleSmallSquare, YES), 100, 24);
    add(@"shadowless square", bezel(@"Copy", NSBezelStyleShadowlessSquare, YES), 100, 24);
    add(@"textured rounded", bezel(@"Folder", NSBezelStyleTexturedRounded, YES), 100, 24);
    add(@"recessed", ({ NSButton *b = bezel(@"Mods", NSBezelStyleRecessed, YES); [b setButtonType:NSButtonTypePushOnPushOff]; [b setBezelStyle:NSBezelStyleRecessed]; [b setState:NSControlStateValueOn]; b; }), 100, 20);
    add(@"round rect", bezel(@"Export", NSBezelStyleRoundRect, YES), 100, 20);
    add(@"inline", bezel(@"12", NSBezelStyleInline, YES), 60, 18);
    add(@"disclosure", ({ NSButton *b = [[NSButton alloc] initWithFrame:NSZeroRect]; [b setButtonType:NSButtonTypeOnOff]; [b setBezelStyle:NSBezelStyleDisclosure]; [b setTitle:@""]; b; }), 21, 21);
    add(@"help", bezel(@"", NSBezelStyleHelpButton, YES), 25, 25);
    add(@"pull-down", ({ NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES]; [p addItemsWithTitles:@[ @"Launch", @"Launch Offline" ]]; p; }), 140, 24);
    add(@"table header", ({ NSTableHeaderView *v = [[NSTableHeaderView alloc] initWithFrame:NSZeroRect]; NSTableColumn *col = [[NSTableColumn alloc] initWithIdentifier:@"name"]; [[col headerCell] setStringValue:@"Name"]; objc_setAssociatedObject(v, "cell", [col headerCell], OBJC_ASSOCIATION_RETAIN); v; }), 200, 24);
    add(@"search small", ({ NSSearchField *s = [[NSSearchField alloc] initWithFrame:NSZeroRect]; [s setControlSize:NSControlSizeSmall]; [s setFont:[NSFont systemFontOfSize:11]]; [s setPlaceholderString:@"Search"]; s; }), 200, 19);
    add(@"progress range 0-0", progress(0, NO, NSProgressIndicatorStyleBar), 240, 20);
    add(@"progress animating", ({ NSProgressIndicator *p = progress(0, YES, NSProgressIndicatorStyleBar); [p startAnimation:nil]; p; }), 240, 20);
    add(@"box bezel border", ({ NSBox *b = [[NSBox alloc] initWithFrame:NSZeroRect]; [b setTitlePosition:NSNoTitle]; [b setBoxType:NSBoxCustom]; [b setBorderType:NSBezelBorder]; b; }), 240, 40);
    add(@"push wide", button(@"Abort", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), 430, 24);
    add(@"push default", ({ NSButton *b = button(@"Launch", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES); [b setKeyEquivalent:@"\r"]; b; }), 100, 24);
    add(@"push disabled", button(@"Kill", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, NO), 100, 24);
    return rows;
}

static NSSegmentedControl *segments(NSArray *labels, NSInteger selected, NSSegmentStyle style)
{
    NSSegmentedControl *s = [NSSegmentedControl segmentedControlWithLabels:labels trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:NULL];

    [s setSegmentStyle:style];
    if (selected >= 0)
        [s setSelectedSegment:selected];
    return s;
}

static NSArray *tabControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"one on automatic", segments(@[ @"General" ], 0, NSSegmentStyleAutomatic), 90, 24);
    add(@"one off automatic", segments(@[ @"Java" ], -1, NSSegmentStyleAutomatic), 90, 24);
    add(@"one on rounded", segments(@[ @"General" ], 0, NSSegmentStyleRounded), 90, 24);
    add(@"one off rounded", segments(@[ @"Java" ], -1, NSSegmentStyleRounded), 90, 24);
    add(@"one on separated", segments(@[ @"General" ], 0, NSSegmentStyleSeparated), 90, 24);
    add(@"one on textured", segments(@[ @"General" ], 0, NSSegmentStyleTexturedSquare), 90, 24);
    add(@"one on capsule", segments(@[ @"General" ], 0, NSSegmentStyleCapsule), 90, 24);
    add(@"per segment", ({ NSSegmentedControl *s = segments(@[ @"General", @"Java", @"Tweaks" ], 0, NSSegmentStyleRounded); objc_setAssociatedObject(s, "persegment", @YES, OBJC_ASSOCIATION_RETAIN); s; }), 240, 24);
    add(@"tab view", ({ NSTabView *t = [[NSTabView alloc] initWithFrame:NSZeroRect]; for (NSString *l in @[ @"General", @"Java", @"Tweaks" ]) { NSTabViewItem *i = [[NSTabViewItem alloc] initWithIdentifier:l]; [i setLabel:l]; [t addTabViewItem:i]; } t; }), 300, 80);
    return rows;
}

static NSString *traitsLine(CTFontRef f)
{
    CFDictionaryRef traits = CTFontCopyTraits(f);
    CFNumberRef number = traits ? CFDictionaryGetValue(traits, kCTFontWeightTrait) : NULL;
    double weight = 0;
    NSString *name = CFBridgingRelease(CTFontCopyPostScriptName(f));
    CGGlyph g[2];
    UniChar c[2] = { 'M', 'i' };
    CGSize adv[2];

    if (number)
        CFNumberGetValue(number, kCFNumberDoubleType, &weight);
    CTFontGetGlyphsForCharacters(f, c, g, 2);
    CTFontGetAdvancesForGlyphs(f, kCTFontOrientationHorizontal, g, adv, 2);
    if (traits)
        CFRelease(traits);
    return [NSString stringWithFormat:@"%@ size=%.1f symbolic=0x%x bold=%d weight=%.3f advM=%.3f advi=%.3f", name, CTFontGetSize(f), CTFontGetSymbolicTraits(f),
                     (CTFontGetSymbolicTraits(f) & kCTFontBoldTrait) != 0, weight, adv[0].width, adv[1].width];
}

// the font paths a toolkit takes for bold and semibold system text
static void printFontTraits(void)
{
    CGFloat sizes[] = { 11, 13, 17 };

    for (int i = 0; i < 3; i++) {
        CGFloat size = sizes[i];
        CTFontRef ui = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, NULL);
        CTFontRef bold = CTFontCreateCopyWithSymbolicTraits(ui, size, NULL, kCTFontBoldTrait, kCTFontBoldTrait);
        NSDictionary *weightTraits = @{ (__bridge id)kCTFontWeightTrait: @(0.3) };
        CTFontDescriptorRef semiDesc = CTFontDescriptorCreateCopyWithAttributes(CTFontCopyFontDescriptor(ui), (__bridge CFDictionaryRef)@{ (__bridge id)kCTFontTraitsAttribute: weightTraits });
        CTFontRef semi = CTFontCreateWithFontDescriptor(semiDesc, size, NULL);

        printf("font ui %s\n", [traitsLine(ui) UTF8String]);
        printf("font copy-bold %s\n", bold ? [traitsLine(bold) UTF8String] : "NULL");
        printf("font desc-weight-0.3 %s\n", [traitsLine(semi) UTF8String]);
        // NSFont names, opened again through CoreText so both systems compare the same way
        printf("font NSFont-bold %s\n", [[[NSFont boldSystemFontOfSize:size] fontName] UTF8String]);
        printf("font NSFont-semibold %s\n", [[[NSFont systemFontOfSize:size weight:NSFontWeightSemibold] fontName] UTF8String]);
        printf("font NSFontManager-bold %s\n", [[[[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:size] toHaveTrait:NSBoldFontMask] fontName] UTF8String]);
        fflush(stdout);
    }
}

static NSArray *fontControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    CGFloat sizes[] = { 11, 13, 17 };
    CGFloat weights[] = { 0, 0.3, 0.4 };
    NSString *names[] = { @"regular", @"semibold", @"bold" };

    for (int w = 0; w < 3; w++) {
        for (int i = 0; i < 3; i++) {
            NSTextField *t = [NSTextField labelWithString:@"Minecraft Log Ungrouped"];

            [t setFont:[NSFont systemFontOfSize:sizes[i] weight:weights[w]]];
            [rows addObject:@[ [NSString stringWithFormat:@"%@ %.0f", names[w], sizes[i]], t, @(300), @(sizes[i] + 8) ]];
        }
    }
    return rows;
}

static void writePNG(NSBitmapImageRep *rep, NSString *path);

// text drawn straight into a flipped bitmap over a checkerboard, translucent colours must blend
static void renderText(NSString *dir)
{
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef cg = CGBitmapContextCreate(NULL, 640, 300, 8, 0, space, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    NSArray *colors = @[ [NSColor labelColor], [NSColor secondaryLabelColor], [NSColor tertiaryLabelColor], [NSColor placeholderTextColor],
                         [NSColor colorWithCalibratedRed:1 green:0 blue:0 alpha:0.5] ];
    NSArray *names = @[ @"label", @"secondary", @"tertiary", @"placeholder", @"red 50%" ];

    CGColorSpaceRelease(space);
    CGContextTranslateCTM(cg, 0, 300);
    CGContextScaleCTM(cg, 1, -1);
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:cg flipped:YES]];
    for (int y = 0; y < 300; y += 10) {
        for (int x = 0; x < 640; x += 10) {
            [(((x + y) / 10) % 2 ? [NSColor colorWithCalibratedWhite:0.9 alpha:1] : [NSColor colorWithCalibratedRed:0.3 green:0.5 blue:0.9 alpha:1]) setFill];
            NSRectFill(NSMakeRect(x, y, 10, 10));
        }
    }
    for (NSUInteger i = 0; i < [colors count]; i++)
        [[NSString stringWithFormat:@"%@ Minecraft Log Ungrouped", names[i]] drawAtPoint:NSMakePoint(10, 10 + i * 30)
                                                                   withAttributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:17], NSForegroundColorAttributeName: colors[i] }];
    CGFloat sizes[] = { 13, 15, 17, 20 };
    for (int i = 0; i < 4; i++)
        [@"Minecraft Log" drawAtPoint:NSMakePoint(10 + i * 150, 170) withAttributes:@{ NSFontAttributeName: [NSFont boldSystemFontOfSize:sizes[i]], NSForegroundColorAttributeName: [NSColor blackColor] }];
    for (int i = 0; i < 4; i++)
        [@"Minecraft Log" drawAtPoint:NSMakePoint(10 + i * 150, 220) withAttributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:sizes[i] weight:NSFontWeightSemibold], NSForegroundColorAttributeName: [NSColor blackColor] }];
    CGFloat small[] = { 11, 12, 13 };
    for (int i = 0; i < 3; i++)
        [@"latest Settings Edit Change Group Export" drawAtPoint:NSMakePoint(10 + i * 210, 265) withAttributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:small[i]], NSForegroundColorAttributeName: [NSColor blackColor] }];
    [NSGraphicsContext restoreGraphicsState];

    CGImageRef image = CGBitmapContextCreateImage(cg);
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
    writePNG(rep, [dir stringByAppendingPathComponent:@"text.png"]);
    CGImageRelease(image);
    CGContextRelease(cg);
}

static NSButton *sized(NSButton *b, NSControlSize size)
{
    [b setControlSize:size];
    [b setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    return b;
}

// states and sizes the main gallery leaves out
static NSArray *extraControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"push small", sized(button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), NSControlSizeSmall), 100, 21);
    add(@"push mini", sized(button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), NSControlSizeMini), 100, 16);
    add(@"push pressed", ({ NSButton *b = button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES); [[b cell] setHighlighted:YES]; b; }), 100, 24);
    add(@"push frame 32", button(@"Copy", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), 100, 32);
    add(@"check small on", sized(button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), NSControlSizeSmall), 160, 18);
    add(@"check mini on", sized(button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES), NSControlSizeMini), 160, 18);
    add(@"check off disabled", button(@"Disabled", NSButtonTypeSwitch, 0, NSControlStateValueOff, NO), 160, 18);
    add(@"check pressed", ({ NSButton *b = button(@"Wrap lines", NSButtonTypeSwitch, 0, NSControlStateValueOn, YES); [[b cell] setHighlighted:YES]; b; }), 160, 18);
    add(@"radio small on", sized(button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, YES), NSControlSizeSmall), 160, 18);
    add(@"radio on disabled", button(@"Option B", NSButtonTypeRadio, 0, NSControlStateValueOn, NO), 160, 18);
    add(@"search with text", ({ NSSearchField *s = [[NSSearchField alloc] initWithFrame:NSZeroRect]; [s setStringValue:@"fabric"]; s; }), 200, 22);
    add(@"popup disabled", ({ NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO]; [p addItemsWithTitles:@[ @"Vulkan" ]]; [p setEnabled:NO]; p; }), 140, 24);
    add(@"slider 70% h28", ({ NSSlider *s = [NSSlider sliderWithValue:70 minValue:0 maxValue:100 target:nil action:NULL]; s; }), 200, 28);
    add(@"segmented second", ({ NSSegmentedControl *s = [NSSegmentedControl segmentedControlWithLabels:@[ @"Log", @"Version", @"Mods" ] trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:NULL]; [s setSelectedSegment:1]; s; }), 200, 24);
    return rows;
}

static NSButton *bezel(NSString *title, NSBezelStyle style, BOOL enabled)
{
    NSButton *b = button(title, NSButtonTypeMomentaryPushIn, style, NSControlStateValueOff, enabled);

    [b setBezelStyle:style];
    return b;
}

// the bezel styles Qt's macOS style draws with, beyond the rounded push button
static NSArray *bezelControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"textured square", bezel(@"Launch", NSBezelStyleTexturedSquare, YES), 100, 24);
    add(@"textured square off", bezel(@"Kill", NSBezelStyleTexturedSquare, NO), 100, 24);
    add(@"small square", bezel(@"Edit", NSBezelStyleSmallSquare, YES), 100, 24);
    add(@"shadowless square", bezel(@"Copy", NSBezelStyleShadowlessSquare, YES), 100, 24);
    add(@"textured rounded", bezel(@"Folder", NSBezelStyleTexturedRounded, YES), 100, 24);
    add(@"recessed", ({ NSButton *b = bezel(@"Mods", NSBezelStyleRecessed, YES); [b setButtonType:NSButtonTypePushOnPushOff]; [b setBezelStyle:NSBezelStyleRecessed]; [b setState:NSControlStateValueOn]; b; }), 100, 20);
    add(@"round rect", bezel(@"Export", NSBezelStyleRoundRect, YES), 100, 20);
    add(@"inline", bezel(@"12", NSBezelStyleInline, YES), 60, 18);
    add(@"disclosure", ({ NSButton *b = [[NSButton alloc] initWithFrame:NSZeroRect]; [b setButtonType:NSButtonTypeOnOff]; [b setBezelStyle:NSBezelStyleDisclosure]; [b setTitle:@""]; b; }), 21, 21);
    add(@"help", bezel(@"", NSBezelStyleHelpButton, YES), 25, 25);
    add(@"pull-down", ({ NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES]; [p addItemsWithTitles:@[ @"Launch", @"Launch Offline" ]]; p; }), 140, 24);
    add(@"table header", ({ NSTableHeaderView *v = [[NSTableHeaderView alloc] initWithFrame:NSZeroRect]; NSTableColumn *col = [[NSTableColumn alloc] initWithIdentifier:@"name"]; [[col headerCell] setStringValue:@"Name"]; objc_setAssociatedObject(v, "cell", [col headerCell], OBJC_ASSOCIATION_RETAIN); v; }), 200, 24);
    add(@"search small", ({ NSSearchField *s = [[NSSearchField alloc] initWithFrame:NSZeroRect]; [s setControlSize:NSControlSizeSmall]; [s setFont:[NSFont systemFontOfSize:11]]; [s setPlaceholderString:@"Search"]; s; }), 200, 19);
    add(@"progress range 0-0", progress(0, NO, NSProgressIndicatorStyleBar), 240, 20);
    add(@"progress animating", ({ NSProgressIndicator *p = progress(0, YES, NSProgressIndicatorStyleBar); [p startAnimation:nil]; p; }), 240, 20);
    add(@"box bezel border", ({ NSBox *b = [[NSBox alloc] initWithFrame:NSZeroRect]; [b setTitlePosition:NSNoTitle]; [b setBoxType:NSBoxCustom]; [b setBorderType:NSBezelBorder]; b; }), 240, 40);
    add(@"push wide", button(@"Abort", NSButtonTypeMomentaryPushIn, NSBezelStyleRounded, NSControlStateValueOff, YES), 430, 24);
    return rows;
}

static NSSegmentedControl *segments(NSArray *labels, NSInteger selected, NSSegmentStyle style)
{
    NSSegmentedControl *s = [NSSegmentedControl segmentedControlWithLabels:labels trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:NULL];

    [s setSegmentStyle:style];
    if (selected >= 0)
        [s setSelectedSegment:selected];
    return s;
}

static NSArray *tabControls(void)
{
    NSMutableArray *rows = [NSMutableArray array];
    void (^add)(NSString *, NSView *, CGFloat, CGFloat) = ^(NSString *name, NSView *view, CGFloat w, CGFloat h) {
        [rows addObject:@[ name, view, @(w), @(h) ]];
    };

    add(@"one on automatic", segments(@[ @"General" ], 0, NSSegmentStyleAutomatic), 90, 24);
    add(@"one off automatic", segments(@[ @"Java" ], -1, NSSegmentStyleAutomatic), 90, 24);
    add(@"one on rounded", segments(@[ @"General" ], 0, NSSegmentStyleRounded), 90, 24);
    add(@"one off rounded", segments(@[ @"Java" ], -1, NSSegmentStyleRounded), 90, 24);
    add(@"one on separated", segments(@[ @"General" ], 0, NSSegmentStyleSeparated), 90, 24);
    add(@"one on textured", segments(@[ @"General" ], 0, NSSegmentStyleTexturedSquare), 90, 24);
    add(@"one on capsule", segments(@[ @"General" ], 0, NSSegmentStyleCapsule), 90, 24);
    add(@"per segment", ({ NSSegmentedControl *s = segments(@[ @"General", @"Java", @"Tweaks" ], 0, NSSegmentStyleRounded); objc_setAssociatedObject(s, "persegment", @YES, OBJC_ASSOCIATION_RETAIN); s; }), 240, 24);
    add(@"tab view", ({ NSTabView *t = [[NSTabView alloc] initWithFrame:NSZeroRect]; for (NSString *l in @[ @"General", @"Java", @"Tweaks" ]) { NSTabViewItem *i = [[NSTabViewItem alloc] initWithIdentifier:l]; [i setLabel:l]; [t addTabViewItem:i]; } t; }), 300, 80);
    return rows;
}

static void writePNG(NSBitmapImageRep *rep, NSString *path)
{
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];

    if (![png writeToFile:path atomically:YES])
        fprintf(stderr, "uigallery: cannot write %s\n", [path fileSystemRepresentation]);
}

// a flipped view to host the controls, so rows run top down like Qt's coordinate space
@interface GalleryView : NSView
@end

@implementation GalleryView
- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)rect
{
    [[NSColor windowBackgroundColor] setFill];
    NSRectFill([self bounds]);
}
@end

static void renderViews(NSArray *rows, NSString *dir)
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, W, H) styleMask:NSWindowStyleMaskTitled
                                                     backing:NSBackingStoreBuffered defer:NO];
    GalleryView *root = [[GalleryView alloc] initWithFrame:NSMakeRect(0, 0, W, H)];
    CGFloat y = 10;

    [window setContentView:root];
    for (NSArray *row in rows) {
        NSTextField *label = [NSTextField labelWithString:row[0]];
        NSView *view = row[1];

        [label setFrame:NSMakeRect(10, y + 2, 180, 17)];
        [view setFrame:NSMakeRect(200, y, [row[2] doubleValue], [row[3] doubleValue])];
        [root addSubview:label];
        [root addSubview:view];
        y += [row[3] doubleValue] + 10;
    }
    [root layoutSubtreeIfNeeded];

    NSBitmapImageRep *rep = [root bitmapImageRepForCachingDisplayInRect:[root bounds]];
    [root cacheDisplayInRect:[root bounds] toBitmapImageRep:rep];
    writePNG(rep, [dir stringByAppendingPathComponent:@"views.png"]);
}

// Qt's way: a CGBitmapContext wrapped in an NSGraphicsContext, cells drawn straight into it
static void renderCellsNamed(NSArray *rows, NSString *dir, BOOL flipped, NSString *name)
{
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef cg = CGBitmapContextCreate(NULL, W, H, 8, 0, space, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    NSGraphicsContext *ctx;
    CGFloat y = 10;

    CGColorSpaceRelease(space);
    if (flipped) {
        CGContextTranslateCTM(cg, 0, H);
        CGContextScaleCTM(cg, 1, -1);
    }
    ctx = [NSGraphicsContext graphicsContextWithCGContext:cg flipped:flipped];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    [[NSColor windowBackgroundColor] setFill];
    NSRectFill(NSMakeRect(0, 0, W, H));

    for (NSArray *row in rows) {
        NSView *view = row[1];
        CGFloat h = [row[3] doubleValue];
        NSRect frame = NSMakeRect(200, flipped ? y : H - y - h, [row[2] doubleValue], h);
        NSRect labelFrame = NSMakeRect(10, flipped ? y + 2 : H - y - 2 - 17, 180, 17);

        [view setFrame:frame];
        [(NSString *)row[0] drawInRect:labelFrame withAttributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                                                                   NSForegroundColorAttributeName: [NSColor labelColor] }];
        if ([view isKindOfClass:[NSControl class]] && [(NSControl *)view cell] != nil)
            [[(NSControl *)view cell] drawWithFrame:frame inView:view];
        y += h + 10;
    }
    [NSGraphicsContext restoreGraphicsState];

    CGImageRef image = CGBitmapContextCreateImage(cg);
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
    writePNG(rep, [dir stringByAppendingPathComponent:name]);
    CGImageRelease(image);
    CGContextRelease(cg);
}

// the panels lay out their views without being shown when AppKit offers the private hook
static void renderPanel(NSSavePanel *panel, NSString *dir, NSString *name)
{
    SEL prepare = NSSelectorFromString(@"_prepareFilePanelForDisplay");

    if (![panel respondsToSelector:prepare])
        return;
    // offscreen, the first layout of an open panel with types draws only part of its views, so it is laid out twice
    [panel performSelector:prepare];
    [panel performSelector:prepare];

    NSView *frame = [[panel contentView] superview];
    NSBitmapImageRep *rep = [frame bitmapImageRepForCachingDisplayInRect:[frame bounds]];

    [frame cacheDisplayInRect:[frame bounds] toBitmapImageRep:rep];
    writePNG(rep, [dir stringByAppendingPathComponent:name]);
}

static void renderPanels(NSString *dir)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *files = [dir stringByAppendingPathComponent:@"panel-files"];
    NSURL *url = [NSURL fileURLWithPath:files isDirectory:YES];

    [fm createDirectoryAtPath:[files stringByAppendingPathComponent:@"Projects"] withIntermediateDirectories:YES attributes:nil error:NULL];
    [fm createDirectoryAtPath:[files stringByAppendingPathComponent:@"Screenshots"] withIntermediateDirectories:YES attributes:nil error:NULL];
    for (NSString *name in @[ @"notes.txt", @"photo.png", @"report.pdf", @"setup.sh" ])
        [@"gallery" writeToFile:[files stringByAppendingPathComponent:name] atomically:NO encoding:NSUTF8StringEncoding error:NULL];

    NSOpenPanel *open = [NSOpenPanel openPanel];
    [open setDirectoryURL:url];
    [open setAllowedFileTypes:@[ @"txt", @"png" ]];
    renderPanel(open, dir, @"panels-open.png");

    NSSavePanel *save = [NSSavePanel savePanel];
    [save setDirectoryURL:url];
    [save setNameFieldStringValue:@"Untitled.txt"];
    [save setMessage:@"Choose where to keep the notes"];
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSNavPanelExpandedStateForSaveMode"];
    renderPanel(save, dir, @"panels-save.png");

    NSSavePanel *expanded = [NSSavePanel savePanel];
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSNavPanelExpandedStateForSaveMode"];
    [expanded setDirectoryURL:url];
    [expanded setNameFieldStringValue:@"Untitled.txt"];
    [expanded setAllowedFileTypes:@[ @"txt", @"rtf" ]];
    renderPanel(expanded, dir, @"panels-save-expanded.png");
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"NSNavPanelExpandedStateForSaveMode"];
}

static void renderCells(NSArray *rows, NSString *dir, BOOL flipped)
{
    renderCellsNamed(rows, dir, flipped, flipped ? @"cells-flipped.png" : @"cells-unflipped.png");
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        NSString *dir = argc > 1 ? [NSString stringWithUTF8String:argv[1]] : @".";

        [NSApplication sharedApplication];
        [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
        [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:NULL];
        renderViews(controls(), dir);
        renderCells(controls(), dir, YES);
        renderCells(controls(), dir, NO);
        renderCellsNamed(extraControls(), dir, YES, @"extras-light.png");
        [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
        [[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua] performAsCurrentDrawingAppearance:^{
            renderCellsNamed(extraControls(), dir, YES, @"extras-dark.png");
        }];
        printf("uigallery: wrote views.png, cells-flipped.png, cells-unflipped.png and the extras to %s\n", [dir fileSystemRepresentation]);
    }
    return 0;
}
