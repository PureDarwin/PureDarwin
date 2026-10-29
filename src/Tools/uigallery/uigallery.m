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
