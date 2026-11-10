// main.m - Screenshot: a floating capture bar, a selection frame and the thumbnail in the corner
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the frame comes from the compositor through swcsnap, the bar crops and saves it

#import <AppKit/AppKit.h>
#import "PDUtility.h"
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

#define BAR_W 356
#define BAR_H 46
#define THUMB_W 210
#define THUMB_H 160

enum { ModeScreen, ModeWindow, ModeSelection };

@class ShotController;

// draws a borderless window's whole content through a block
@interface DrawView : NSView {
@public
    void (^draw)(NSRect bounds);
    void (^click)(NSPoint p, NSEvent *e);
    void (^drag)(NSPoint p);
    void (^up)(NSPoint p);
}
@end

@implementation DrawView

- (void)dealloc
{
    [draw release];
    [click release];
    [drag release];
    [up release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)drawRect:(NSRect)dirty
{
    if (draw != nil)
        draw([self bounds]);
}

- (void)mouseDown:(NSEvent *)event
{
    if (click != nil)
        click([self convertPoint:[event locationInWindow] fromView:nil], event);
}

- (void)mouseDragged:(NSEvent *)event
{
    if (drag != nil)
        drag([self convertPoint:[event locationInWindow] fromView:nil]);
}

- (void)mouseUp:(NSEvent *)event
{
    if (up != nil)
        up([self convertPoint:[event locationInWindow] fromView:nil]);
}

@end

@interface ShotController : NSObject {
    NSWindow *_bar;
    NSWindow *_overlay;
    NSWindow *_thumb;
    DrawView *_barView;
    int _mode;
    NSRect _selection;
    NSPoint _dragFrom;
    NSString *_saveTo;
    int _timer;
    int _countdown;
    BOOL _pointer;
    NSMutableArray *_rects;
    NSImage *_last;
    NSString *_lastPath;
}
@end

static NSWindow *borderless(NSRect frame, NSInteger level)
{
    NSWindow *w = [[NSWindow alloc] initWithContentRect:frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];

    [w setOpaque:NO];
    [w setBackgroundColor:[NSColor clearColor]];
    [w setLevel:level];
    [w setHasShadow:NO];
    return w;
}

// swcsnap ships with neuswc, it asks the compositor for the current frame as a ppm
static NSString *snapTool(void)
{
    for (NSString *p in @[ @"/bin/swcsnap", @"/usr/bin/swcsnap", @"/usr/local/bin/swcsnap" ]) {
        if ([[NSFileManager defaultManager] isExecutableFileAtPath:p])
            return p;
    }
    for (NSString *dir in [[[[NSProcessInfo processInfo] environment] objectForKey:@"PATH"] componentsSeparatedByString:@":"]) {
        NSString *p = [dir stringByAppendingPathComponent:@"swcsnap"];

        if ([[NSFileManager defaultManager] isExecutableFileAtPath:p])
            return p;
    }
    return nil;
}

static NSBitmapImageRep *readPPM(NSString *path)
{
    NSData *data = [NSData dataWithContentsOfFile:path];
    const unsigned char *b = [data bytes];
    NSUInteger len = [data length], pos = 2;
    int vals[3], n = 0;
    NSBitmapImageRep *rep;

    if (len < 3 || b[0] != 'P' || b[1] != '6')
        return nil;
    // width, height and the maximum value, with comments allowed between them
    while (n < 3 && pos < len) {
        if (b[pos] == '#') {
            while (pos < len && b[pos] != '\n')
                pos++;
        } else if (b[pos] >= '0' && b[pos] <= '9') {
            int v = 0;
            while (pos < len && b[pos] >= '0' && b[pos] <= '9')
                v = v * 10 + (b[pos++] - '0');
            vals[n++] = v;
            continue;
        }
        pos++;
    }
    // one whitespace byte ends the header
    pos++;
    if (n < 3 || pos > len || len - pos < (NSUInteger)vals[0] * vals[1] * 3)
        return nil;
    // four samples per pixel, the PNG encoder reads three-sample rows shifted
    rep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:vals[0] pixelsHigh:vals[1] bitsPerSample:8 samplesPerPixel:4
                                                    hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:vals[0] * 4 bitsPerPixel:32] autorelease];
    for (int y = 0; y < vals[1]; y++) {
        const unsigned char *src = b + pos + (NSUInteger)y * vals[0] * 3;
        unsigned char *dst = [rep bitmapData] + y * [rep bytesPerRow];

        for (int x = 0; x < vals[0]; x++) {
            dst[x * 4] = src[x * 3];
            dst[x * 4 + 1] = src[x * 3 + 1];
            dst[x * 4 + 2] = src[x * 3 + 2];
            dst[x * 4 + 3] = 255;
        }
    }
    return rep;
}

@implementation ShotController

- (NSRect)screen
{
    return [[NSScreen mainScreen] frame];
}

// the selection in screen coordinates, top-left origin like the captured frame
- (NSRect)captureRect
{
    NSRect s = [self screen];

    if (_mode == ModeScreen || NSIsEmptyRect(_selection))
        return NSMakeRect(0, 0, NSWidth(s), NSHeight(s));
    return _selection;
}

- (void)drawBar:(NSRect)b
{
    NSArray *modes = @[ @"screen", @"window", @"selection" ];
    NSDictionary *a = PDText(PDFont(@"Inter-Regular", 12), PDFixed(0xf0f0f0));
    NSDictionary *cap = PDText(PDFont(@"Inter-SemiBold", 12), PDFixed(0xffffff));
    CGFloat x = 6;

    PDFillRounded(b, 12, [NSColor colorWithCalibratedRed:40 / 255.0 green:40 / 255.0 blue:42 / 255.0 alpha:.92]);
    [_rects removeAllObjects];
    NSBezierPath *cross = [NSBezierPath bezierPath];
    [cross moveToPoint:NSMakePoint(x + 9, 19)];
    [cross lineToPoint:NSMakePoint(x + 17, 27)];
    [cross moveToPoint:NSMakePoint(x + 17, 19)];
    [cross lineToPoint:NSMakePoint(x + 9, 27)];
    [cross setLineWidth:1.5];
    [cross setLineCapStyle:NSRoundLineCapStyle];
    [PDFixed(0xa0a0a0) setStroke];
    [cross stroke];
    [_rects addObject:[NSValue valueWithRect:NSMakeRect(x, 6, 26, 34)]];
    x += 32;
    [PDAlpha(PDFixed(0xffffff), .18) setFill];
    NSRectFill(NSMakeRect(x, 10, 1, 26));
    x += 8;
    for (int i = 0; i < 3; i++) {
        NSRect r = NSMakeRect(x, 6, 38, 34);

        if (i == _mode)
            PDFillRounded(r, 8, PDAlpha(PDFixed(0xffffff), .2));
        PDDrawGlyph(modes[i], NSMakeRect(NSMidX(r) - 11, NSMidY(r) - 11, 22, 22), i == ModeWindow ? PDFixed(0x6a6a6c) : PDFixed(0xf0f0f0));
        [_rects addObject:[NSValue valueWithRect:r]];
        x += 40;
    }
    x += 4;
    [PDAlpha(PDFixed(0xffffff), .18) setFill];
    NSRectFill(NSMakeRect(x, 10, 1, 26));
    x += 8;
    NSString *opt = NSLocalizedString(@"Options", @"");
    NSSize os = [opt sizeWithAttributes:a];
    NSBezierPath *tri = [NSBezierPath bezierPath];
    [opt drawAtPoint:NSMakePoint(x + 8, floor(23 - os.height / 2)) withAttributes:a];
    [tri moveToPoint:NSMakePoint(x + 13 + os.width, 21)];
    [tri lineToPoint:NSMakePoint(x + 21 + os.width, 21)];
    [tri lineToPoint:NSMakePoint(x + 17 + os.width, 26)];
    [tri closePath];
    [PDFixed(0xf0f0f0) setFill];
    [tri fill];
    [_rects addObject:[NSValue valueWithRect:NSMakeRect(x, 6, os.width + 28, 34)]];
    x += os.width + 32;
    NSString *c = _countdown > 0 ? [NSString stringWithFormat:@"%d", _countdown] : NSLocalizedString(@"Capture", @"");
    NSSize cs = [c sizeWithAttributes:cap];
    NSRect cr = NSMakeRect(NSMaxX(b) - cs.width - 38, 8, cs.width + 32, 30);
    PDFillRounded(cr, 8, [NSColor controlAccentColor]);
    [c drawAtPoint:NSMakePoint(NSMidX(cr) - cs.width / 2, floor(NSMidY(cr) - cs.height / 2)) withAttributes:cap];
    [_rects addObject:[NSValue valueWithRect:cr]];
}

- (void)drawOverlay:(NSRect)b
{
    NSRect r = _selection;
    NSBezierPath *frame;
    float dash[2] = { 4, 3 };
    NSString *dims;
    NSDictionary *a = PDText(PDMono(11), PDFixed(0xffffff));
    NSSize ds;

    if (_mode != ModeSelection)
        return;
    // the dim goes round the selection in four strips, the selection itself stays untouched
    [PDAlpha([NSColor blackColor], .32) setFill];
    NSRectFillUsingOperation(NSMakeRect(0, 0, NSWidth(b), NSMinY(r)), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(0, NSMaxY(r), NSWidth(b), NSHeight(b) - NSMaxY(r)), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(0, NSMinY(r), NSMinX(r), NSHeight(r)), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(NSMaxX(r), NSMinY(r), NSWidth(b) - NSMaxX(r), NSHeight(r)), NSCompositeSourceOver);
    frame = [NSBezierPath bezierPathWithRect:NSInsetRect(r, .5, .5)];
    [frame setLineWidth:1];
    [PDAlpha([NSColor blackColor], .4) setStroke];
    [frame stroke];
    [frame setLineDash:dash count:2 phase:0];
    [[NSColor whiteColor] setStroke];
    [frame stroke];
    for (int i = 0; i < 2; i++) {
        NSPoint c = i ? NSMakePoint(NSMaxX(r), NSMaxY(r)) : r.origin;
        PDFillRounded(NSMakeRect(c.x - 4, c.y - 4, 8, 8), 4, [NSColor whiteColor]);
    }
    dims = [NSString stringWithFormat:@"%.0f × %.0f", NSWidth(r), NSHeight(r)];
    ds = [dims sizeWithAttributes:a];
    PDFillRounded(NSMakeRect(NSMaxX(r) - ds.width - 14, NSMaxY(r) + 8, ds.width + 14, ds.height + 4), 5, PDAlpha([NSColor blackColor], .6));
    [dims drawAtPoint:NSMakePoint(NSMaxX(r) - ds.width - 7, NSMaxY(r) + 10) withAttributes:a];
    if (_countdown > 0) {
        NSDictionary *big = PDText(PDFont(@"Inter-Thin", 96), [NSColor whiteColor]);
        NSString *n = [NSString stringWithFormat:@"%d", _countdown];
        NSSize ns = [n sizeWithAttributes:big];
        [n drawAtPoint:NSMakePoint(NSMidX(b) - ns.width / 2, NSHeight(b) * .4 - ns.height / 2) withAttributes:big];
    }
}

- (void)showOverlay
{
    if (_mode == ModeSelection)
        [_overlay orderFront:nil];
    else
        [_overlay orderOut:nil];
    [[_overlay contentView] setNeedsDisplay:YES];
    [_bar orderFront:nil];
}

- (void)setMode:(int)mode
{
    // which window a click lands on is the compositor's to say, until it tells us that mode stays off
    if (mode == ModeWindow)
        return;
    _mode = mode;
    if (_mode == ModeSelection && NSIsEmptyRect(_selection)) {
        NSRect s = [self screen];
        _selection = NSMakeRect(floor(NSWidth(s) * .28), floor(NSHeight(s) * .2), floor(NSWidth(s) * .4), floor(NSHeight(s) * .42));
    }
    [self showOverlay];
    [_barView setNeedsDisplay:YES];
}

- (NSString *)saveFolder
{
    NSString *home = NSHomeDirectory();

    if ([_saveTo isEqualToString:@"Documents"])
        return [home stringByAppendingPathComponent:@"Documents"];
    if ([_saveTo isEqualToString:@"Pictures"])
        return [home stringByAppendingPathComponent:@"Pictures"];
    return [home stringByAppendingPathComponent:@"Desktop/Screenshots"];
}

- (void)options:(NSEvent *)event
{
    NSMenu *m = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    NSArray *places = @[ @[ @"Desktop", NSLocalizedString(@"Desktop", @"") ], @[ @"Documents", NSLocalizedString(@"Documents", @"") ],
                         @[ @"Pictures", NSLocalizedString(@"Pictures", @"") ], @[ @"Clipboard", NSLocalizedString(@"Clipboard", @"") ] ];

    [[m addItemWithTitle:NSLocalizedString(@"Save to", @"") action:NULL keyEquivalent:@""] setEnabled:NO];
    for (NSArray *p in places) {
        NSMenuItem *i = [m addItemWithTitle:p[1] action:@selector(chooseFolder:) keyEquivalent:@""];
        [i setTarget:self];
        [i setRepresentedObject:p[0]];
        [i setState:[_saveTo isEqualToString:p[0]] ? NSOnState : NSOffState];
    }
    [m addItem:[NSMenuItem separatorItem]];
    [[m addItemWithTitle:NSLocalizedString(@"Timer", @"") action:NULL keyEquivalent:@""] setEnabled:NO];
    for (NSNumber *t in @[ @0, @5, @10 ]) {
        NSMenuItem *i = [m addItemWithTitle:[t intValue] == 0 ? NSLocalizedString(@"None", @"") : [NSString stringWithFormat:NSLocalizedString(@"%d Seconds", @""), [t intValue]]
                                     action:@selector(chooseTimer:) keyEquivalent:@""];
        [i setTarget:self];
        [i setTag:[t intValue]];
        [i setState:_timer == [t intValue] ? NSOnState : NSOffState];
    }
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem *ptr = [m addItemWithTitle:NSLocalizedString(@"Show Mouse Pointer", @"") action:@selector(togglePointer:) keyEquivalent:@""];
    [ptr setTarget:self];
    [ptr setState:_pointer ? NSOnState : NSOffState];
    [NSMenu popUpContextMenu:m withEvent:event forView:_barView];
}

- (IBAction)chooseFolder:(id)sender
{
    [_saveTo release];
    _saveTo = [[sender representedObject] copy];
    [[NSUserDefaults standardUserDefaults] setObject:_saveTo forKey:@"SaveTo"];
}

- (IBAction)chooseTimer:(id)sender
{
    _timer = (int)[(NSMenuItem *)sender tag];
    [[NSUserDefaults standardUserDefaults] setInteger:_timer forKey:@"Timer"];
}

- (IBAction)togglePointer:(id)sender
{
    _pointer = !_pointer;
}

- (IBAction)captureScreen:(id)sender
{
    [self setMode:ModeScreen];
    [self capture:nil];
}

- (IBAction)captureSelection:(id)sender
{
    [self setMode:ModeSelection];
}

- (IBAction)showBar:(id)sender
{
    [self showOverlay];
}

- (IBAction)showScreenshotsFolder:(id)sender
{
    [[NSFileManager defaultManager] createDirectoryAtPath:[self saveFolder] withIntermediateDirectories:YES attributes:nil error:NULL];
    [[NSWorkspace sharedWorkspace] launchApplication:@"/System/Library/CoreServices/Finder.app"];
}

- (void)tick:(NSTimer *)t
{
    if (--_countdown > 0) {
        [[_overlay contentView] setNeedsDisplay:YES];
        [_barView setNeedsDisplay:YES];
        return;
    }
    [t invalidate];
    [self grab];
}

- (IBAction)capture:(id)sender
{
    if (_timer > 0) {
        _countdown = _timer;
        [_overlay orderFront:nil];
        [[_overlay contentView] setNeedsDisplay:YES];
        [_barView setNeedsDisplay:YES];
        [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
        return;
    }
    [self grab];
}

// our own windows go first, then the compositor gets a moment to draw the screen without them
- (void)grab
{
    [_bar orderOut:nil];
    [_overlay orderOut:nil];
    [_thumb orderOut:nil];
    [self performSelector:@selector(snap) withObject:nil afterDelay:.35];
}

- (void)fail:(NSString *)why
{
    NSLog(@"Screenshot: %@", why);
    NSBeep();
    [self showOverlay];
}

- (void)snap
{
    NSString *tool = snapTool(), *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:@"screenshot.ppm"];
    const char *argv[] = { "swcsnap", "image", [tmp fileSystemRepresentation], NULL };
    pid_t pid;
    int status = 0;
    NSBitmapImageRep *full, *cut;
    NSRect r;

    if (tool == nil) {
        [self fail:@"swcsnap is not installed"];
        return;
    }
    if (posix_spawn(&pid, [tool fileSystemRepresentation], NULL, NULL, (char *const *)argv, environ) != 0 || waitpid(pid, &status, 0) < 0 || status != 0) {
        [self fail:@"the compositor did not hand over a frame"];
        return;
    }
    full = readPPM(tmp);
    unlink([tmp fileSystemRepresentation]);
    if (full == nil) {
        [self fail:@"the frame could not be read"];
        return;
    }

    r = NSIntersectionRect(NSIntegralRect([self captureRect]), NSMakeRect(0, 0, [full pixelsWide], [full pixelsHigh]));
    cut = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:NSWidth(r) pixelsHigh:NSHeight(r) bitsPerSample:8 samplesPerPixel:4
                                                    hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:NSWidth(r) * 4 bitsPerPixel:32] autorelease];
    for (int y = 0; y < NSHeight(r); y++)
        memcpy([cut bitmapData] + y * [cut bytesPerRow], [full bitmapData] + (NSUInteger)(y + NSMinY(r)) * [full bytesPerRow] + (NSUInteger)NSMinX(r) * 4, NSWidth(r) * 4);
    [self saveRep:cut];
}

- (void)saveRep:(NSBitmapImageRep *)rep
{
    NSDateFormatter *f = [[[NSDateFormatter alloc] init] autorelease];
    NSData *png = [rep representationUsingType:NSPNGFileType properties:@{}];
    NSString *dir = [self saveFolder], *name, *path = nil;

    [f setDateFormat:@"yyyy-MM-dd 'at' HH.mm.ss"];
    name = [NSString stringWithFormat:NSLocalizedString(@"Screenshot %@.png", @""), [f stringFromDate:[NSDate date]]];
    if ([_saveTo isEqualToString:@"Clipboard"]) {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        [pb declareTypes:@[ NSTIFFPboardType ] owner:nil];
        [pb setData:[rep TIFFRepresentation] forType:NSTIFFPboardType];
    } else {
        [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:NULL];
        path = [dir stringByAppendingPathComponent:name];
        if (![png writeToFile:path atomically:YES]) {
            [self fail:[NSString stringWithFormat:@"could not write %@", path]];
            return;
        }
    }
    [_last release];
    _last = [[NSImage alloc] init];
    [_last addRepresentation:rep];
    [_lastPath release];
    _lastPath = [path copy];
    [self showThumbnail:path != nil ? name : NSLocalizedString(@"Copied to Clipboard", @"") size:NSMakeSize([rep pixelsWide], [rep pixelsHigh])];
}

- (void)showThumbnail:(NSString *)name size:(NSSize)size
{
    NSRect s = [[NSScreen mainScreen] visibleFrame];
    NSString *info = [NSString stringWithFormat:@"%.0f × %.0f PNG%@", size.width, size.height,
                      _lastPath != nil ? [@", " stringByAppendingString:[[_lastPath stringByDeletingLastPathComponent] stringByAbbreviatingWithTildeInPath]] : @""];
    DrawView *v;

    [_thumb release];
    _thumb = borderless(NSMakeRect(NSMaxX(s) - THUMB_W - 18, NSMinY(s) + 90, THUMB_W, THUMB_H), NSFloatingWindowLevel);
    v = [[[DrawView alloc] initWithFrame:NSMakeRect(0, 0, THUMB_W, THUMB_H)] autorelease];
    v->draw = [^(NSRect b) {
        NSRect im = NSMakeRect(0, 0, NSWidth(b), 112);
        PDFillRounded(b, 10, PDColor(0xffffff, 0x2c2c2e));
        [NSGraphicsContext saveGraphicsState];
        [[NSBezierPath bezierPathWithRoundedRect:b xRadius:10 yRadius:10] addClip];
        [_last drawInRect:im fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        [NSGraphicsContext restoreGraphicsState];
        [PDFitted(name, PDText(PDFont(@"Inter-SemiBold", 11), PDColor(0x1b1b1b, 0xeeeeee)), NSWidth(b) - 20)
            drawAtPoint:NSMakePoint(10, 118) withAttributes:PDText(PDFont(@"Inter-SemiBold", 11), PDColor(0x1b1b1b, 0xeeeeee))];
        [PDFitted(info, PDText(PDFont(@"Inter-Regular", 11), PDColor(0x555555, 0xa0a0a0)), NSWidth(b) - 20)
            drawAtPoint:NSMakePoint(10, 135) withAttributes:PDText(PDFont(@"Inter-Regular", 11), PDColor(0x555555, 0xa0a0a0))];
    } copy];
    v->click = [^(NSPoint p, NSEvent *e) {
        if (_lastPath != nil)
            [[NSWorkspace sharedWorkspace] openFile:_lastPath withApplication:@"Image Viewer"];
        [_thumb orderOut:nil];
    } copy];
    [_thumb setContentView:v];
    [_thumb orderFront:nil];
    [self performSelector:@selector(thumbDone) withObject:nil afterDelay:6];
}

// the thumbnail stays a few seconds, then the app is done unless the bar is wanted again
- (void)thumbDone
{
    [_thumb orderOut:nil];
    if (![_bar isVisible])
        [NSApp terminate:self];
}

- (void)barClick:(NSPoint)p event:(NSEvent *)e
{
    for (NSUInteger i = 0; i < [_rects count]; i++) {
        if (!NSPointInRect(p, [_rects[i] rectValue]))
            continue;
        if (i == 0)
            [NSApp terminate:self];
        else if (i <= 3)
            [self setMode:(int)i - 1];
        else if (i == 4)
            [self options:e];
        else
            [self capture:nil];
        return;
    }
    [_bar performWindowDragWithEvent:e];
}

- (void)buildMenu
{
    NSMenu *windowMenu;
    NSMenu *main = PDStandardMenus(NSLocalizedString(@"Screenshot", @""), &windowMenu);
    NSMenu *cap = PDAddMenu(main, NSLocalizedString(@"Capture", @""));
    NSMenu *opt = PDAddMenu(main, NSLocalizedString(@"Options", @""));

    [PDAddItem(cap, NSLocalizedString(@"Entire Screen", @""), @selector(captureScreen:), @"3", NSCommandKeyMask | NSShiftKeyMask) setTarget:self];
    [PDAddItem(cap, NSLocalizedString(@"Window", @""), NULL, @"", 0) setEnabled:NO];
    [PDAddItem(cap, NSLocalizedString(@"Selection", @""), @selector(captureSelection:), @"4", NSCommandKeyMask | NSShiftKeyMask) setTarget:self];
    [cap addItem:[NSMenuItem separatorItem]];
    [PDAddItem(cap, NSLocalizedString(@"Capture", @""), @selector(capture:), @"\r", 0) setTarget:self];
    [PDAddItem(opt, NSLocalizedString(@"Show Capture Bar", @""), @selector(showBar:), @"5", NSCommandKeyMask | NSShiftKeyMask) setTarget:self];
    [PDAddItem(opt, NSLocalizedString(@"Show Screenshots Folder", @""), @selector(showScreenshotsFolder:), @"", 0) setTarget:self];
    [NSApp setMainMenu:main];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect s = [self screen], vis = [[NSScreen mainScreen] visibleFrame];
    DrawView *ov;

    _rects = [[NSMutableArray alloc] init];
    _saveTo = [[[NSUserDefaults standardUserDefaults] stringForKey:@"SaveTo"] ?: @"Desktop" copy];
    _timer = (int)[[NSUserDefaults standardUserDefaults] integerForKey:@"Timer"];
    _mode = ModeSelection;
    [self buildMenu];

    _overlay = borderless(s, NSScreenSaverWindowLevel - 1);
    ov = [[[DrawView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(s), NSHeight(s))] autorelease];
    ov->draw = [^(NSRect b) { [self drawOverlay:b]; } copy];
    // the compositor may hand the bar's clicks to the overlay under it, so those go on to the bar
    ov->click = [^(NSPoint p, NSEvent *e) {
        NSRect bf = [_bar frame];
        NSPoint q = NSMakePoint(p.x - NSMinX(bf), p.y - (NSHeight(s) - NSMaxY(bf)));

        if ([_bar isVisible] && NSPointInRect(q, NSMakeRect(0, 0, NSWidth(bf), NSHeight(bf)))) {
            [self barClick:q event:e];
            return;
        }
        _dragFrom = p;
    } copy];
    ov->drag = [^(NSPoint p) {
        _selection = NSMakeRect(MIN(p.x, _dragFrom.x), MIN(p.y, _dragFrom.y), fabs(p.x - _dragFrom.x), fabs(p.y - _dragFrom.y));
        [[_overlay contentView] setNeedsDisplay:YES];
    } copy];
    [_overlay setContentView:ov];

    _bar = borderless(NSMakeRect(floor(NSMidX(vis) - BAR_W / 2), NSMinY(vis) + 84, BAR_W, BAR_H), NSScreenSaverWindowLevel);
    _barView = [[DrawView alloc] initWithFrame:NSMakeRect(0, 0, BAR_W, BAR_H)];
    _barView->draw = [^(NSRect b) { [self drawBar:b]; } copy];
    _barView->click = [^(NSPoint p, NSEvent *e) { [self barClick:p event:e]; } copy];
    [_bar setContentView:_barView];

    // -screen captures straight away, for scripts and the keyboard shortcut
    if ([[[NSProcessInfo processInfo] arguments] containsObject:@"-screen"]) {
        _mode = ModeScreen;
        [self grab];
        return;
    }
    [self setMode:_mode];
    [_bar makeKeyAndOrderFront:nil];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    ShotController *controller = [[ShotController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
