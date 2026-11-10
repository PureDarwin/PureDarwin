// SoundMenu.m - the Sound menu: the default output's level, the output devices and Sound Settings
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// all through the CoreAudio HAL and its listeners

#import "SoundMenu.h"
#import "PDWiFiUI.h"
#import "MenuBarView.h"
#import <CoreAudio/CoreAudio.h>

#define MenuW 292
#define Inset 5
#define HeadH 32
#define SliderH 34
#define TitleH 24
#define ItemH 24
#define SepH 11

NSString *const SoundChangedNotification = @"SoundChangedNotification";

static NSColor *c(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static BOOL dark(void)
{
    CFPropertyListRef style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);

    if (style == NULL)
        return NO;
    CFRelease(style);
    return YES;
}

static NSDictionary *t(NSString *face, CGFloat size, NSColor *colour)
{
    NSFont *f = [NSFont fontWithName:face size:size] ?: [NSFont systemFontOfSize:size];
    return @{ NSFontAttributeName: f, NSForegroundColorAttributeName: colour };
}

static AudioObjectPropertyAddress addr(AudioObjectPropertySelector sel, AudioObjectPropertyScope scope)
{
    return (AudioObjectPropertyAddress){ sel, scope, kAudioObjectPropertyElementMain };
}

static AudioDeviceID defaultOutput(void)
{
    AudioObjectPropertyAddress a = addr(kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID d = kAudioObjectUnknown;
    UInt32 size = sizeof(d);

    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &d);
    return d;
}

static Float32 volumeOf(AudioDeviceID d)
{
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
    Float32 v = 0;
    UInt32 size = sizeof(v);

    AudioObjectGetPropertyData(d, &a, 0, NULL, &size, &v);
    return v;
}

static BOOL mutedOf(AudioDeviceID d)
{
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
    UInt32 v = 0, size = sizeof(v);

    AudioObjectGetPropertyData(d, &a, 0, NULL, &size, &v);
    return v != 0;
}

static NSString *nameOf(AudioDeviceID d)
{
    AudioObjectPropertyAddress a = addr(kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal);
    CFStringRef s = NULL;
    UInt32 size = sizeof(s);

    if (AudioObjectGetPropertyData(d, &a, 0, NULL, &size, &s) != noErr || s == NULL)
        return @"";
    return [(NSString *)s autorelease];
}

// the devices with an output stream, in coreaudiod's order
static NSArray *outputDevices(void)
{
    AudioObjectPropertyAddress a = addr(kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID ids[32];
    UInt32 size = sizeof(ids);
    NSMutableArray *out = [NSMutableArray array];

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, ids) != noErr)
        return out;
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); i++) {
        AudioObjectPropertyAddress s = addr(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput);
        UInt32 n = 0;

        if (AudioObjectGetPropertyDataSize(ids[i], &s, 0, NULL, &n) == noErr && n > 0)
            [out addObject:@(ids[i])];
    }
    return out;
}

// the HAL calls listeners on its own thread, the menu and the bar redraw on the main one
static OSStatus soundChanged(AudioObjectID object, UInt32 n, const AudioObjectPropertyAddress *a, void *data);

@interface SoundItem : NSObject {
@public
    NSString *kind;
    NSString *title;
    AudioDeviceID device;
    CGFloat y, h;
}
@end

@implementation SoundItem
- (void)dealloc
{
    [kind release];
    [title release];
    [super dealloc];
}
@end

@interface SoundMenuView : NSView {
@public
    NSMutableArray *_items;
    NSInteger _hover;
    SoundMenu *_owner;
    BOOL _dragging;
}
- (CGFloat)rebuild;
@end

@interface SoundMenu () {
    NSPanel *_panel;
    SoundMenuView *_view;
    CGFloat _x;
    NSMutableSet *_watched;
}
- (void)close;
- (void)relayout;
- (void)watchDevices;
@end

static OSStatus soundChanged(AudioObjectID object, UInt32 n, const AudioObjectPropertyAddress *a, void *data)
{
    [[SoundMenu shared] performSelectorOnMainThread:@selector(changed:) withObject:nil waitUntilDone:NO];
    return noErr;
}

@implementation SoundMenuView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)add:(NSString *)kind title:(NSString *)title device:(AudioDeviceID)device h:(CGFloat)h at:(CGFloat *)y
{
    SoundItem *i = [[[SoundItem alloc] init] autorelease];

    i->kind = [kind retain];
    i->title = [title retain];
    i->device = device;
    i->y = *y;
    i->h = h;
    [_items addObject:i];
    *y += h;
}

- (CGFloat)rebuild
{
    NSArray *devices = outputDevices();
    CGFloat y = Inset;

    if (_items == nil)
        _items = [[NSMutableArray alloc] init];
    [_items removeAllObjects];
    [self add:@"head" title:NSLocalizedString(@"Sound", @"") device:0 h:HeadH at:&y];
    if ([devices count] == 0) {
        [self add:@"note" title:NSLocalizedString(@"No output devices", @"") device:0 h:ItemH at:&y];
    } else {
        [self add:@"slider" title:nil device:defaultOutput() h:SliderH at:&y];
        [self add:@"sep" title:nil device:0 h:SepH at:&y];
        [self add:@"title" title:NSLocalizedString(@"Output", @"") device:0 h:TitleH at:&y];
        for (NSNumber *d in devices)
            [self add:@"device" title:nameOf([d unsignedIntValue]) device:[d unsignedIntValue] h:ItemH at:&y];
    }
    [self add:@"sep" title:nil device:0 h:SepH at:&y];
    [self add:@"settings" title:NSLocalizedString(@"Sound Settings…", @"") device:0 h:ItemH at:&y];
    return y + Inset;
}

- (BOOL)clickable:(SoundItem *)i
{
    return [i->kind isEqualToString:@"device"] || [i->kind isEqualToString:@"settings"];
}

- (NSRect)speakerRect:(SoundItem *)i
{
    return NSMakeRect(Inset + 8, i->y + (i->h - 16) / 2, 16, 16);
}

- (NSRect)trackRect:(SoundItem *)i
{
    CGFloat left = NSMaxX([self speakerRect:i]) + 12;
    return NSMakeRect(left, i->y + (i->h - 6) / 2, NSWidth([self bounds]) - Inset - 14 - left, 6);
}

// a speaker with as many waves as the level reaches, struck through when muted
static void drawSpeaker(NSRect r, Float32 level, BOOL muted, NSColor *ink)
{
    NSBezierPath *body = [NSBezierPath bezierPath];
    CGFloat x = NSMinX(r), y = NSMinY(r), h = NSHeight(r);

    [ink set];
    [body moveToPoint:NSMakePoint(x + 1, y + h * 0.35)];
    [body lineToPoint:NSMakePoint(x + 4, y + h * 0.35)];
    [body lineToPoint:NSMakePoint(x + 8, y + h * 0.1)];
    [body lineToPoint:NSMakePoint(x + 8, y + h * 0.9)];
    [body lineToPoint:NSMakePoint(x + 4, y + h * 0.65)];
    [body lineToPoint:NSMakePoint(x + 1, y + h * 0.65)];
    [body closePath];
    [body fill];
    if (muted) {
        NSBezierPath *x1 = [NSBezierPath bezierPath];
        [x1 setLineWidth:1.5];
        [x1 moveToPoint:NSMakePoint(x + 10, y + h * 0.3)];
        [x1 lineToPoint:NSMakePoint(x + 15, y + h * 0.7)];
        [x1 moveToPoint:NSMakePoint(x + 15, y + h * 0.3)];
        [x1 lineToPoint:NSMakePoint(x + 10, y + h * 0.7)];
        [x1 stroke];
        return;
    }
    for (int w = 0; w < 3 && level > w / 3.0f; w++) {
        NSBezierPath *arc = [NSBezierPath bezierPath];
        [arc setLineWidth:1.3];
        [arc appendBezierPathWithArcWithCenter:NSMakePoint(x + 8, y + h / 2) radius:3 + w * 2.6 startAngle:-45 endAngle:45];
        [arc stroke];
    }
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSColor *ink = dark() ? c(0xee, 0xee, 0xee, 1) : c(0x1b, 0x1b, 0x1b, 1);
    NSColor *muted = c(0x8a, 0x8a, 0x8a, 1), *accent = PDSystemAccent();
    NSBezierPath *frame = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 0.5, 0.5) xRadius:8 yRadius:8];
    AudioDeviceID current = defaultOutput();

    [[NSColor clearColor] set];
    NSRectFillUsingOperation(b, NSCompositeCopy);
    [(dark() ? c(44, 44, 46, 0.96) : c(250, 247, 245, 0.97)) setFill];
    [frame fill];
    [(dark() ? c(255, 255, 255, 0.12) : c(0, 0, 0, 0.12)) setStroke];
    [frame stroke];

    for (NSUInteger k = 0; k < [_items count]; k++) {
        SoundItem *i = _items[k];
        NSRect r = NSMakeRect(Inset, i->y, NSWidth(b) - 2 * Inset, i->h);
        BOOL hot = (NSInteger)k == _hover && [self clickable:i];
        NSColor *fg = hot ? [NSColor whiteColor] : ink;

        if (hot) {
            [accent setFill];
            [[NSBezierPath bezierPathWithRoundedRect:r xRadius:5 yRadius:5] fill];
        }
        if ([i->kind isEqualToString:@"head"]) {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMidY(r) - 8) withAttributes:t(@"Inter-SemiBold", 13, ink)];
        } else if ([i->kind isEqualToString:@"slider"]) {
            Float32 level = volumeOf(i->device);
            BOOL off = mutedOf(i->device);
            NSRect track = [self trackRect:i], fill = track;
            CGFloat knobX = NSMinX(track) + level * NSWidth(track);

            drawSpeaker([self speakerRect:i], level, off, ink);
            [(dark() ? c(255, 255, 255, 0.18) : c(0, 0, 0, 0.12)) setFill];
            [[NSBezierPath bezierPathWithRoundedRect:track xRadius:3 yRadius:3] fill];
            fill.size.width = knobX - NSMinX(track);
            [(off ? muted : accent) setFill];
            [[NSBezierPath bezierPathWithRoundedRect:fill xRadius:3 yRadius:3] fill];
            [[NSColor whiteColor] setFill];
            NSBezierPath *knob = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(knobX - 8, NSMidY(track) - 8, 16, 16)];
            [knob fill];
            [c(0, 0, 0, 0.25) setStroke];
            [knob stroke];
        } else if ([i->kind isEqualToString:@"title"]) {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMinY(r) + 7) withAttributes:t(@"Inter-SemiBold", 11, muted)];
        } else if ([i->kind isEqualToString:@"note"]) {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMidY(r) - 8) withAttributes:t(@"Inter-Regular", 13, muted)];
        } else if ([i->kind isEqualToString:@"sep"]) {
            [(dark() ? c(255, 255, 255, 0.14) : c(0, 0, 0, 0.12)) setFill];
            // a path fill blends, NSRectFill would punch the panel through to the desktop
            [[NSBezierPath bezierPathWithRect:NSMakeRect(NSMinX(r) + 3, NSMidY(r), NSWidth(r) - 6, 1)] fill];
        } else if ([i->kind isEqualToString:@"device"]) {
            if (i->device == current)
                [@"✓" drawAtPoint:NSMakePoint(NSMinX(r) + 6, NSMidY(r) - 8) withAttributes:t(@"Inter-Regular", 13, fg)];
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 24, NSMidY(r) - 8) withAttributes:t(@"Inter-Regular", 13, fg)];
        } else {
            [i->title drawAtPoint:NSMakePoint(NSMinX(r) + 5, NSMidY(r) - 8) withAttributes:t(@"Inter-Regular", 13, fg)];
        }
    }
}

- (NSInteger)itemAt:(NSPoint)p
{
    for (NSUInteger k = 0; k < [_items count]; k++) {
        SoundItem *i = _items[k];
        if (p.y >= i->y && p.y < i->y + i->h)
            return (NSInteger)k;
    }
    return -1;
}

- (void)mouseMoved:(NSEvent *)e
{
    NSInteger h = [self itemAt:[self convertPoint:[e locationInWindow] fromView:nil]];

    if (h != _hover) {
        _hover = h;
        [self setNeedsDisplay:YES];
    }
}

- (SoundItem *)slider
{
    for (SoundItem *i in _items) {
        if ([i->kind isEqualToString:@"slider"])
            return i;
    }
    return nil;
}

// the level follows the pointer along the track, a level above nothing also unmutes
- (void)setLevelAt:(NSPoint)p
{
    SoundItem *i = [self slider];
    NSRect track = [self trackRect:i];
    Float32 v = (Float32)((p.x - NSMinX(track)) / NSWidth(track));
    AudioObjectPropertyAddress a = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);

    v = v < 0 ? 0 : v > 1 ? 1 : v;
    AudioObjectSetPropertyData(defaultOutput(), &a, 0, NULL, sizeof(v), &v);
    if (v > 0 && mutedOf(defaultOutput())) {
        AudioObjectPropertyAddress m = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
        UInt32 off = 0;
        AudioObjectSetPropertyData(defaultOutput(), &m, 0, NULL, sizeof(off), &off);
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)e
{
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    NSInteger k = [self itemAt:p];
    SoundItem *i = k >= 0 ? _items[k] : nil;

    if (i == nil)
        return;
    if ([i->kind isEqualToString:@"slider"]) {
        if (NSPointInRect(p, NSInsetRect([self speakerRect:i], -4, -6))) {
            AudioObjectPropertyAddress m = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
            UInt32 on = !mutedOf(i->device);
            AudioObjectSetPropertyData(i->device, &m, 0, NULL, sizeof(on), &on);
            [self setNeedsDisplay:YES];
            return;
        }
        _dragging = YES;
        [self setLevelAt:p];
    } else if ([i->kind isEqualToString:@"device"]) {
        AudioObjectPropertyAddress a = addr(kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal);
        AudioDeviceID d = i->device;
        AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, sizeof(d), &d);
        [_owner relayout];
    } else if ([i->kind isEqualToString:@"settings"]) {
        [_owner close];
        // Settings opens on the pane named here, as the menu bar's other shortcuts do
        CFPreferencesSetAppValue(CFSTR("OpenPane"), CFSTR("sound"), CFSTR("org.puredarwin.Settings"));
        CFPreferencesAppSynchronize(CFSTR("org.puredarwin.Settings"));
        [[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/Settings.app"];
    }
}

- (void)mouseDragged:(NSEvent *)e
{
    if (_dragging)
        [self setLevelAt:[self convertPoint:[e locationInWindow] fromView:nil]];
}

- (void)mouseUp:(NSEvent *)e
{
    _dragging = NO;
}

- (void)keyDown:(NSEvent *)e
{
    if ([[e characters] isEqualToString:@"\e"])
        [_owner close];
}

- (void)dealloc
{
    [_items release];
    [super dealloc];
}

@end

@implementation SoundMenu

+ (SoundMenu *)shared
{
    static SoundMenu *shared;

    if (shared == nil) {
        shared = [[SoundMenu alloc] init];
        [shared watchDevices];
    }
    return shared;
}

// the system's list and defaults, and the level of every device, the list grows as devices arrive
- (void)watchDevices
{
    AudioObjectPropertySelector system[] = { kAudioHardwarePropertyDevices, kAudioHardwarePropertyDefaultOutputDevice };

    if (_watched == nil) {
        _watched = [[NSMutableSet alloc] init];
        for (int k = 0; k < 2; k++) {
            AudioObjectPropertyAddress a = addr(system[k], kAudioObjectPropertyScopeGlobal);
            AudioObjectAddPropertyListener(kAudioObjectSystemObject, &a, soundChanged, NULL);
        }
    }
    for (NSNumber *d in outputDevices()) {
        if ([_watched containsObject:d])
            continue;
        AudioObjectPropertyAddress v = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
        AudioObjectPropertyAddress m = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
        AudioObjectAddPropertyListener([d unsignedIntValue], &v, soundChanged, NULL);
        AudioObjectAddPropertyListener([d unsignedIntValue], &m, soundChanged, NULL);
        [_watched addObject:d];
    }
}

- (BOOL)isOpen
{
    return _panel != nil && [_panel isVisible];
}

- (BOOL)hasOutput
{
    return defaultOutput() != kAudioObjectUnknown;
}

- (BOOL)silent
{
    AudioDeviceID d = defaultOutput();
    return d != kAudioObjectUnknown && (mutedOf(d) || volumeOf(d) <= 0);
}

- (void)relayout
{
    NSRect screen = [[NSScreen mainScreen] frame];
    CGFloat h;

    if (![self isOpen])
        return;
    h = [_view rebuild];
    [_panel setFrame:NSMakeRect(MIN(_x, NSMaxX(screen) - MenuW - 6), NSMaxY(screen) - MenuBarHeight - 2 - h, MenuW, h) display:NO];
    [_view setFrame:NSMakeRect(0, 0, MenuW, h)];
    [_view setNeedsDisplay:YES];
}

- (void)changed:(id)unused
{
    [self watchDevices];
    [self relayout];
    [[NSNotificationCenter defaultCenter] postNotificationName:SoundChangedNotification object:nil];
}

- (void)resigned:(NSNotification *)note
{
    [self close];
}

- (void)toggleAtX:(CGFloat)x
{
    if ([self isOpen]) {
        [self close];
        return;
    }
    if (_panel == nil) {
        _panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, MenuW, 100) styleMask:NSBorderlessWindowMask
                                              backing:NSBackingStoreBuffered defer:NO];
        [_panel setLevel:NSModalPanelWindowLevel];
        [_panel setOpaque:NO];
        [_panel setBackgroundColor:[NSColor clearColor]];
        [_panel setAcceptsMouseMovedEvents:YES];
        _view = [[SoundMenuView alloc] initWithFrame:NSMakeRect(0, 0, MenuW, 100)];
        _view->_owner = self;
        [_panel setContentView:_view];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(resigned:) name:NSWindowDidResignKeyNotification
                                                   object:_panel];
    }
    _x = x;
    _view->_hover = -1;
    [_panel orderFront:nil];
    [self relayout];
    [_panel makeKeyAndOrderFront:nil];
    [_panel makeFirstResponder:_view];
}

- (void)close
{
    [_panel orderOut:nil];
    // the bar drops the glyph's highlight
    [[NSNotificationCenter defaultCenter] postNotificationName:SoundChangedNotification object:nil];
}

@end
