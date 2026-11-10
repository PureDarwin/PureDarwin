// main.m - Image Viewer: a picture on a dark stage, its folder as a strip of thumbnails
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>
#import "PDUtility.h"

#define STRIP_H 66
#define THUMB_W 72
#define THUMB_H 44

@interface ViewerView : NSView {
    NSMutableArray *_files;
    NSUInteger _index;
    NSImage *_image;
    NSSize _pixels;
    NSString *_kind;
    unsigned long long _size;
    CGFloat _zoom;
    int _rotation;
    NSArray *_buttons;
    PDToolButton *_prev;
    PDToolButton *_next;
    NSMutableDictionary *_thumbs;
}
- (void)openPath:(NSString *)path;
- (IBAction)previousImage:(id)sender;
- (IBAction)nextImage:(id)sender;
- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;
- (IBAction)zoomToFit:(id)sender;
- (IBAction)actualSize:(id)sender;
- (IBAction)rotateLeft:(id)sender;
- (IBAction)rotateRight:(id)sender;
- (IBAction)setAsWallpaper:(id)sender;
- (IBAction)showInFiles:(id)sender;
@end

static BOOL isImage(NSString *name)
{
    return [@[ @"png", @"jpg", @"jpeg", @"gif", @"tif", @"tiff", @"bmp" ] containsObject:[[name pathExtension] lowercaseString]];
}

@implementation ViewerView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _files = [[NSMutableArray alloc] init];
    _thumbs = [[NSMutableDictionary alloc] init];
    _prev = [PDToolButton buttonWithGlyph:@"back" label:nil target:self action:@selector(previousImage:)];
    _next = [PDToolButton buttonWithGlyph:@"forward" label:nil target:self action:@selector(nextImage:)];
    _buttons = [@[ _prev, _next,
                   [PDToolButton buttonWithGlyph:@"zoomout" label:nil target:self action:@selector(zoomOut:)],
                   [PDToolButton buttonWithGlyph:nil label:@"100%" target:self action:@selector(actualSize:)],
                   [PDToolButton buttonWithGlyph:@"zoomin" label:nil target:self action:@selector(zoomIn:)],
                   [PDToolButton buttonWithGlyph:@"fit" label:nil target:self action:@selector(zoomToFit:)],
                   [PDToolButton buttonWithGlyph:@"rotl" label:nil target:self action:@selector(rotateLeft:)],
                   [PDToolButton buttonWithGlyph:@"rotr" label:nil target:self action:@selector(rotateRight:)] ] retain];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

- (void)dealloc
{
    [_files release];
    [_thumbs release];
    [_image release];
    [_kind release];
    [_buttons release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (NSString *)current
{
    return _index < [_files count] ? _files[_index] : nil;
}

- (void)load
{
    NSString *path = [self current];
    NSBitmapImageRep *rep;

    [_image release];
    _image = path != nil ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
    rep = (NSBitmapImageRep *)[[_image representations] firstObject];
    _pixels = [rep isKindOfClass:[NSBitmapImageRep class]] ? NSMakeSize([rep pixelsWide], [rep pixelsHigh]) : [_image size];
    _size = [[[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL] fileSize];
    [_kind release];
    _kind = [[[[path pathExtension] uppercaseString] stringByReplacingOccurrencesOfString:@"JPG" withString:@"JPEG"] copy];
    _zoom = 0;
    _rotation = 0;
    [[self window] setTitle:path != nil ? [path lastPathComponent] : NSLocalizedString(@"Image Viewer", @"")];
    [self setNeedsDisplay:YES];
}

// the folder's pictures in name order, the one opened selected
- (void)openPath:(NSString *)path
{
    NSString *dir = [path stringByDeletingLastPathComponent];
    NSArray *names = [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:NULL] sortedArrayUsingComparator:^NSComparisonResult(NSString *a, NSString *b) { return [a compare:b options:NSNumericSearch | NSCaseInsensitiveSearch]; }];

    [_files removeAllObjects];
    for (NSString *n in names) {
        if (isImage(n) && ![n hasPrefix:@"."])
            [_files addObject:[dir stringByAppendingPathComponent:n]];
    }
    if (![_files containsObject:path])
        [_files insertObject:path atIndex:0];
    _index = [_files indexOfObject:path];
    [self load];
}

- (NSRect)stageRect
{
    NSRect b = [self bounds];

    return NSMakeRect(0, PD_TOOLBAR_H + 1, NSWidth(b), NSHeight(b) - PD_TOOLBAR_H - 1 - STRIP_H - PD_STATUS_H);
}

- (CGFloat)fitScale
{
    NSRect s = [self stageRect];
    NSSize px = _rotation % 180 ? NSMakeSize(_pixels.height, _pixels.width) : _pixels;

    if (px.width < 1 || px.height < 1)
        return 1;
    return MIN(1, MIN((NSWidth(s) - 40) / px.width, (NSHeight(s) - 40) / px.height));
}

- (CGFloat)scale
{
    return _zoom > 0 ? _zoom : [self fitScale];
}

- (NSImage *)thumbFor:(NSString *)path
{
    NSImage *t = _thumbs[path];

    if (t == nil) {
        t = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        _thumbs[path] = t != nil ? (id)t : (id)[NSNull null];
    }
    return [t isKindOfClass:[NSImage class]] ? t : nil;
}

- (void)drawImageIn:(NSRect)stage
{
    CGFloat s = [self scale];
    NSSize drawn = NSMakeSize(_pixels.width * s, _pixels.height * s);
    NSAffineTransform *t = [NSAffineTransform transform];
    NSShadow *shadow = [[[NSShadow alloc] init] autorelease];

    if (_image == nil)
        return;
    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:stage];
    [t translateXBy:NSMidX(stage) yBy:NSMidY(stage)];
    [t rotateByDegrees:_rotation];
    [t concat];
    [shadow setShadowBlurRadius:24];
    [shadow setShadowOffset:NSMakeSize(0, -6)];
    [shadow setShadowColor:PDAlpha([NSColor blackColor], .5)];
    [shadow set];
    [_image drawInRect:NSMakeRect(-drawn.width / 2, -drawn.height / 2, drawn.width, drawn.height) fromRect:NSZeroRect
             operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawStrip:(NSRect)strip
{
    NSUInteger n = [_files count], first, count;
    CGFloat x;

    [PDFixed(0x19191a) setFill];
    NSRectFill(strip);
    [PDFixed(0x111111) setFill];
    NSRectFill(NSMakeRect(0, NSMinY(strip), NSWidth(strip), 1));
    // as many thumbnails as fit, centred on the current one
    count = MIN(n, (NSUInteger)MAX(1, (NSWidth(strip) - 20) / (THUMB_W + 8)));
    first = _index > count / 2 ? MIN(_index - count / 2, n - count) : 0;
    x = floor(NSMidX(strip) - (count * (THUMB_W + 8) - 8) / 2);
    for (NSUInteger i = first; i < first + count; i++) {
        NSRect r = NSMakeRect(x, NSMidY(strip) - THUMB_H / 2, THUMB_W, THUMB_H);
        NSImage *t = [self thumbFor:_files[i]];

        [NSGraphicsContext saveGraphicsState];
        [[NSBezierPath bezierPathWithRoundedRect:r xRadius:4 yRadius:4] addClip];
        [t drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:i == _index ? 1 : .65 respectFlipped:YES hints:nil];
        [NSGraphicsContext restoreGraphicsState];
        if (i == _index) {
            NSBezierPath *ring = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, -1, -1) xRadius:5 yRadius:5];
            [ring setLineWidth:2];
            [[NSColor controlAccentColor] setStroke];
            [ring stroke];
        }
        x += THUMB_W + 8;
    }
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds], stage = [self stageRect];
    NSString *path = [self current];
    NSSize px = _rotation % 180 ? NSMakeSize(_pixels.height, _pixels.width) : _pixels;
    PDToolButton *pct = _buttons[3];

    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:8 yRadius:8] addClip];
    // the stage is dark in both looks so pictures read the same
    for (int row = 0; row < PD_TOOLBAR_H; row++) {
        [[NSColor colorWithCalibratedWhite:(0x3b - 9.0 * row / (PD_TOOLBAR_H - 1)) / 255.0 alpha:1] setFill];
        NSRectFill(NSMakeRect(0, row, NSWidth(b), 1));
    }
    [PDFixed(0x1c1c1c) setFill];
    NSRectFill(NSMakeRect(0, PD_TOOLBAR_H, NSWidth(b), 1));
    PDDrawTitle(path != nil ? [path lastPathComponent] : NSLocalizedString(@"Image Viewer", @""),
                path != nil ? [[path stringByDeletingLastPathComponent] stringByAbbreviatingWithTildeInPath] : nil,
                NSMakePoint(PD_LIGHTS_W, PD_TOOLBAR_H / 2), NSWidth(b) - PD_LIGHTS_W - 330, YES);
    _prev->enabled = _index > 0;
    _next->enabled = _index + 1 < [_files count];
    [pct->label release];
    pct->label = [[NSString stringWithFormat:@"%.0f%%", [self scale] * 100] copy];
    PDLayoutButtons(_buttons, NSWidth(b) - 10, PD_TOOLBAR_H, YES);

    [PDFixed(0x1e1e20) setFill];
    NSRectFill(stage);
    [self drawImageIn:stage];
    [self drawStrip:NSMakeRect(0, NSMaxY(stage), NSWidth(b), STRIP_H)];

    [PDFixed(0x19191a) setFill];
    NSRectFill(NSMakeRect(0, NSHeight(b) - PD_STATUS_H, NSWidth(b), PD_STATUS_H));
    if (path != nil) {
        NSDictionary *a = PDText(PDFont(@"Inter-Regular", 11), PDFixed(0x8a8a8a));
        NSString *left = [NSString stringWithFormat:NSLocalizedString(@"%.0f × %.0f   %@ image   %@", @""), px.width, px.height, _kind, PDBytes(_size)];
        NSString *right = [NSString stringWithFormat:NSLocalizedString(@"%lu of %lu in %@", @""), (unsigned long)_index + 1, (unsigned long)[_files count],
                           [[path stringByDeletingLastPathComponent] lastPathComponent]];
        CGFloat y = NSHeight(b) - PD_STATUS_H + 5;

        [left drawAtPoint:NSMakePoint(12, y) withAttributes:a];
        [right drawAtPoint:NSMakePoint(NSWidth(b) - 12 - [right sizeWithAttributes:a].width, y) withAttributes:a];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSRect stage = [self stageRect];

    if (PDClickButtons(_buttons, p))
        return;
    if (p.y < PD_TOOLBAR_H) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    if (p.y > NSMaxY(stage) && p.y < NSMaxY(stage) + STRIP_H) {
        NSUInteger n = [_files count], count = MIN(n, (NSUInteger)MAX(1, (NSWidth(stage) - 20) / (THUMB_W + 8)));
        NSUInteger first = _index > count / 2 ? MIN(_index - count / 2, n - count) : 0;
        CGFloat x = floor(NSMidX(stage) - (count * (THUMB_W + 8) - 8) / 2);
        NSInteger i = (NSInteger)floor((p.x - x) / (THUMB_W + 8));

        if (i >= 0 && i < (NSInteger)count) {
            _index = first + i;
            [self load];
        }
        return;
    }
    if ([event clickCount] == 2)
        _zoom > 0 ? [self zoomToFit:nil] : [self actualSize:nil];
}

- (void)keyDown:(NSEvent *)event
{
    unichar c = [[event charactersIgnoringModifiers] length] ? [[event charactersIgnoringModifiers] characterAtIndex:0] : 0;

    if (c == NSLeftArrowFunctionKey)
        [self previousImage:nil];
    else if (c == NSRightArrowFunctionKey || c == ' ')
        [self nextImage:nil];
    else
        [super keyDown:event];
}

- (void)scrollWheel:(NSEvent *)event
{
    if ([event deltaY] > 0)
        [self zoomIn:nil];
    else if ([event deltaY] < 0)
        [self zoomOut:nil];
}

- (IBAction)previousImage:(id)sender
{
    if (_index > 0) {
        _index--;
        [self load];
    }
}

- (IBAction)nextImage:(id)sender
{
    if (_index + 1 < [_files count]) {
        _index++;
        [self load];
    }
}

- (IBAction)zoomIn:(id)sender
{
    _zoom = MIN(8, [self scale] * 1.25);
    [self setNeedsDisplay:YES];
}

- (IBAction)zoomOut:(id)sender
{
    _zoom = MAX(.02, [self scale] * .8);
    [self setNeedsDisplay:YES];
}

- (IBAction)zoomToFit:(id)sender
{
    _zoom = 0;
    [self setNeedsDisplay:YES];
}

- (IBAction)actualSize:(id)sender
{
    _zoom = 1;
    [self setNeedsDisplay:YES];
}

- (IBAction)rotateLeft:(id)sender
{
    _rotation = (_rotation + 270) % 360;
    [self setNeedsDisplay:YES];
}

- (IBAction)rotateRight:(id)sender
{
    _rotation = (_rotation + 90) % 360;
    [self setNeedsDisplay:YES];
}

// the Dock reads DesktopPicture as a name in Desktop Pictures or a full path
- (IBAction)setAsWallpaper:(id)sender
{
    if ([self current] == nil)
        return;
    CFPreferencesSetAppValue(CFSTR("DesktopPicture"), (CFStringRef)[self current], CFSTR("org.puredarwin.Dock"));
    CFPreferencesAppSynchronize(CFSTR("org.puredarwin.Dock"));
}

- (IBAction)showInFiles:(id)sender
{
    [[NSWorkspace sharedWorkspace] launchApplication:@"/System/Library/CoreServices/Finder.app"];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    SEL a = [item action];

    if (a == @selector(previousImage:))
        return _index > 0;
    if (a == @selector(nextImage:))
        return _index + 1 < [_files count];
    return [self current] != nil;
}

@end

@interface ViewerController : NSObject {
    NSMutableArray *_windows;
}
@end

@implementation ViewerController

- (void)buildMenu
{
    NSMenu *windowMenu;
    NSMenu *main = PDStandardMenus(NSLocalizedString(@"Image Viewer", @""), &windowMenu);
    NSMenu *file = PDAddMenu(main, NSLocalizedString(@"File", @""));
    NSMenu *view = PDAddMenu(main, NSLocalizedString(@"View", @""));
    NSMenu *image = PDAddMenu(main, NSLocalizedString(@"Image", @""));
    NSMenu *go = PDAddMenu(main, NSLocalizedString(@"Go", @""));

    PDAddItem(file, NSLocalizedString(@"Show in Files", @""), @selector(showInFiles:), @"", 0);
    PDAddItem(file, NSLocalizedString(@"Set as Wallpaper", @""), @selector(setAsWallpaper:), @"", 0);
    [file addItem:[NSMenuItem separatorItem]];
    PDAddItem(file, NSLocalizedString(@"Close", @""), @selector(performClose:), @"w", 0);
    PDAddItem(view, NSLocalizedString(@"Zoom to Fit", @""), @selector(zoomToFit:), @"9", 0);
    PDAddItem(view, NSLocalizedString(@"Actual Size", @""), @selector(actualSize:), @"0", 0);
    PDAddItem(view, NSLocalizedString(@"Zoom In", @""), @selector(zoomIn:), @"+", 0);
    PDAddItem(view, NSLocalizedString(@"Zoom Out", @""), @selector(zoomOut:), @"-", 0);
    PDAddItem(image, NSLocalizedString(@"Rotate Left", @""), @selector(rotateLeft:), @"l", 0);
    PDAddItem(image, NSLocalizedString(@"Rotate Right", @""), @selector(rotateRight:), @"r", 0);
    PDAddItem(go, NSLocalizedString(@"Previous Image", @""), @selector(previousImage:), [NSString stringWithFormat:@"%C", (unichar)NSLeftArrowFunctionKey], 0);
    PDAddItem(go, NSLocalizedString(@"Next Image", @""), @selector(nextImage:), [NSString stringWithFormat:@"%C", (unichar)NSRightArrowFunctionKey], 0);
    [PDAddItem(main, NSLocalizedString(@"Window", @""), NULL, @"", 0) setSubmenu:windowMenu];
    [NSApp setMainMenu:main];
}

- (void)openPath:(NSString *)path
{
    NSWindow *w = PDMakeWindow(NSMakeSize(760, 540), NSMakeSize(420, 300), 24.0 * [_windows count], self);
    ViewerView *v = [[[ViewerView alloc] initWithFrame:[[w contentView] bounds]] autorelease];

    [w setContentView:v];
    [_windows addObject:w];
    if (path != nil)
        [v openPath:path];
    [w makeKeyAndOrderFront:nil];
    [w makeFirstResponder:v];
}

- (void)windowWillClose:(NSNotification *)note
{
    [[[note object] retain] autorelease];
    [_windows removeObject:[note object]];
    if ([_windows count] == 0)
        [NSApp terminate:self];
}

// opened from the Dock it shows the first picture in ~/Pictures
- (NSString *)firstPicture
{
    for (NSString *dir in @[ [NSHomeDirectory() stringByAppendingPathComponent:@"Pictures"], @"/System/Library/Desktop Pictures" ]) {
        for (NSString *n in [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:NULL] sortedArrayUsingComparator:^NSComparisonResult(NSString *a, NSString *b) { return [a compare:b options:NSNumericSearch | NSCaseInsensitiveSearch]; }]) {
            if (isImage(n))
                return [dir stringByAppendingPathComponent:n];
        }
    }
    return nil;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    NSUInteger opened = 0;

    _windows = [[NSMutableArray alloc] init];
    [self buildMenu];
    for (NSUInteger i = 1; i < [args count]; i++) {
        if ([args[i] hasPrefix:@"-"])
            continue;
        [self openPath:args[i]];
        opened++;
    }
    if (opened == 0)
        [self openPath:[self firstPicture]];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    ViewerController *controller = [[ViewerController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
