/*
 * AboutPanel.m - the About PureDarwin window opened from the beak menu.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import "AboutPanel.h"
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/attr.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>

#define PANEL_W 280
#define PANEL_H 384
#define ROW_H 16

static NSFont *font(NSString *name, CGFloat size)
{
    NSFont *f = [NSFont fontWithName:name size:size];

    return f != nil ? f : [NSFont systemFontOfSize:size];
}

// greys follow the appearance, light value to dark value after the demo's dark windows
static NSColor *grey(int v)
{
    static const int dark[][2] = {
        { 0x1b, 0xe6 }, { 0x4a, 0xc8 }, { 0x6e, 0xa0 }, { 0x8a, 0x96 }, { 0xec, 0x26 }, { 0xc4, 0x55 },
        { 0xfa, 0x3a }, { 0xe2, 0x14 }, { 0xd6, 0x14 },
    };
    int night = v;

    for (unsigned i = 0; i < sizeof(dark) / sizeof(dark[0]); i++)
        if (dark[i][0] == v)
            night = dark[i][1];
    return [NSColor colorWithName:nil dynamicProvider:^NSColor *(NSAppearance *a) {
        return [NSColor colorWithCalibratedWhite:([a _isDark] ? night : v) / 255.0 alpha:1];
    }];
}

static BOOL darkNow(void)
{
    return [[NSAppearance currentDrawingAppearance] _isDark];
}

static NSString *sysctlString(const char *name)
{
    char buf[512];
    size_t len = sizeof(buf);

    if (sysctlbyname(name, buf, &len, NULL, 0) != 0 || len == 0)
        return nil;
    buf[sizeof(buf) - 1] = 0;
    return [NSString stringWithUTF8String:buf];
}

// the first xnu-<digits> in kern.version, a plain scan since that's all it takes
static NSRange xnuTagRange(NSString *version)
{
    const char *s = [version UTF8String], *p = s;

    while (s != NULL && (p = strstr(p, "xnu-")) != NULL) {
        if (p[4] >= '0' && p[4] <= '9')
            return NSMakeRange(p - s, 4);
        p += 4;
    }
    return NSMakeRange(NSNotFound, 0);
}

// "Darwin 25.5.0 (xnu-12377)", the xnu tag cut from kern.version at its first dot
static NSString *kernelVersion(void)
{
    struct utsname u;
    NSString *version = sysctlString("kern.version");
    NSRange xnu = version != nil ? xnuTagRange(version) : NSMakeRange(NSNotFound, 0);
    NSString *tag = nil;

    if (uname(&u) != 0)
        return @"Darwin";
    if (xnu.location != NSNotFound) {
        NSString *rest = [version substringFromIndex:xnu.location];
        NSRange end = [rest rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@".~/: "]];

        tag = end.location != NSNotFound ? [rest substringToIndex:end.location] : rest;
    }
    if (tag != nil)
        return [NSString stringWithFormat:@"%s %s (%@)", u.sysname, u.release, tag];
    return [NSString stringWithFormat:@"%s %s", u.sysname, u.release];
}

static NSString *processor(void)
{
    struct utsname u;
    int cpus = 0;
    size_t len = sizeof(cpus);

    sysctlbyname("hw.ncpu", &cpus, &len, NULL, 0);
    uname(&u);
    return [NSString stringWithFormat:@"%d-core %s", cpus > 0 ? cpus : 1, u.machine];
}

static NSString *memory(void)
{
    uint64_t bytes = 0;
    size_t len = sizeof(bytes);

    if (sysctlbyname("hw.memsize", &bytes, &len, NULL, 0) != 0 || bytes == 0)
        return @"Unknown";
    return [NSString stringWithFormat:@"%llu GB", (unsigned long long)((bytes + (1ULL << 29)) >> 30)];
}

static uint32_t dataWord(CFTypeRef data)
{
    uint32_t v = 0;

    if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID() && CFDataGetLength(data) >= 4)
        CFDataGetBytes(data, CFRangeMake(0, 4), (UInt8 *)&v);
    return v;
}

// the first PCI display controller, by its model string or else its vendor
static NSString *graphics(void)
{
    io_iterator_t iter = IO_OBJECT_NULL;
    io_service_t dev;
    NSString *result = nil;

    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching("IOPCIDevice"), &iter) != KERN_SUCCESS)
        return @"Unknown";
    while (result == nil && (dev = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        CFTypeRef cls = IORegistryEntryCreateCFProperty(dev, CFSTR("class-code"), kCFAllocatorDefault, 0);
        CFTypeRef model = IORegistryEntryCreateCFProperty(dev, CFSTR("model"), kCFAllocatorDefault, 0);
        CFTypeRef vendor = IORegistryEntryCreateCFProperty(dev, CFSTR("vendor-id"), kCFAllocatorDefault, 0);

        if ((dataWord(cls) >> 16) == 0x03) {
            if (model != NULL && CFGetTypeID(model) == CFDataGetTypeID())
                result = [[[NSString alloc] initWithBytes:CFDataGetBytePtr(model) length:strnlen((const char *)CFDataGetBytePtr(model), CFDataGetLength(model))
                                                 encoding:NSUTF8StringEncoding] autorelease];
            else if (model != NULL && CFGetTypeID(model) == CFStringGetTypeID())
                result = [[(NSString *)model copy] autorelease];
            if ([result length] == 0) {
                switch (dataWord(vendor) & 0xffff) {
                    case 0x1002: result = @"AMD Radeon"; break;
                    case 0x10de: result = @"NVIDIA"; break;
                    case 0x8086: result = @"Intel Graphics"; break;
                    case 0x1af4: result = @"Virtio GPU"; break;
                    case 0x1234: result = @"QEMU Standard VGA"; break;
                    case 0x15ad: result = @"VMware SVGA"; break;
                    default: result = @"Display controller"; break;
                }
            }
        }
        if (cls != NULL)
            CFRelease(cls);
        if (model != NULL)
            CFRelease(model);
        if (vendor != NULL)
            CFRelease(vendor);
        IOObjectRelease(dev);
    }
    IOObjectRelease(iter);
    return result != nil ? result : @"Unknown";
}

// the root volume's name, PureDarwin when the filesystem keeps none
static NSString *startupDisk(void)
{
    struct attrlist list = { ATTR_BIT_MAP_COUNT, 0, 0, ATTR_VOL_INFO | ATTR_VOL_NAME, 0, 0, 0 };
    struct {
        uint32_t length;
        attrreference_t name;
        char buf[256];
    } __attribute__((packed)) reply;

    if (getattrlist("/", &list, &reply, sizeof(reply), 0) == 0 && reply.name.attr_length > 1) {
        NSString *name = [NSString stringWithUTF8String:(char *)&reply.name + reply.name.attr_dataoffset];

        if ([name length] > 0 && ![name isEqualToString:@"/"])
            return name;
    }
    return @"PureDarwin";
}

@interface AboutView : NSView {
    NSArray *_rows;
    NSString *_version;
    NSRect _button;
    BOOL _pressed;
}
@end

@implementation AboutView

- (instancetype)initWithFrame:(NSRect)frame
{
    NSDictionary *sv = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];

    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _version = [[NSString alloc] initWithFormat:@"Version %@ (%@)", sv[@"ProductVersion"] ?: @"?", sv[@"ProductBuildVersion"] ?: @"?"];
    _rows = [@[
        @[ @"Kernel", kernelVersion() ],
        @[ @"Processor", processor() ],
        @[ @"Memory", memory() ],
        @[ @"Graphics", graphics() ],
        @[ @"Startup Disk", startupDisk() ],
    ] retain];
    return self;
}

- (void)dealloc
{
    [_rows release];
    [_version release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawCentered:(NSString *)text at:(CGFloat)y attributes:(NSDictionary *)attrs
{
    NSSize size = [text sizeWithAttributes:attrs];

    [text drawAtPoint:NSMakePoint(floor((PANEL_W - size.width) / 2), y) withAttributes:attrs];
}

- (void)drawRect:(NSRect)dirty
{
    NSImage *logo = [[[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"beak-large" ofType:@"png"]] autorelease];
    NSDictionary *key = @{ NSFontAttributeName: font(@"Inter-SemiBold", 11), NSForegroundColorAttributeName: grey(0x1b) };
    NSDictionary *value = @{ NSFontAttributeName: font(@"Inter-Regular", 11), NSForegroundColorAttributeName: grey(0x4a) };
    NSDictionary *fine = @{ NSFontAttributeName: font(@"Inter-Regular", 10), NSForegroundColorAttributeName: grey(0x8a) };
    NSDictionary *label = @{ NSFontAttributeName: font(@"Inter-Regular", 12), NSForegroundColorAttributeName: grey(0x1b) };
    CGFloat keyW = 0, valueW = 0, x, y;
    NSSize size;

    [[NSBezierPath bezierPathWithRoundedRect:[self bounds] xRadius:8 yRadius:8] addClip];
    [grey(0xec) setFill];
    NSRectFill([self bounds]);

    [logo drawInRect:NSMakeRect((PANEL_W - 96) / 2, 40, 96, 96) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    [self drawCentered:@"PureDarwin" at:146 attributes:@{ NSFontAttributeName: font(@"Inter-Bold", 22), NSForegroundColorAttributeName: grey(0x1b) }];
    [self drawCentered:_version at:176 attributes:@{ NSFontAttributeName: font(@"Inter-Regular", 11), NSForegroundColorAttributeName: grey(0x6e) }];

    // two columns, names right-aligned against the values, the pair centred like the demo's grid
    for (NSArray *row in _rows) {
        keyW = MAX(keyW, [row[0] sizeWithAttributes:key].width);
        valueW = MAX(valueW, [row[1] sizeWithAttributes:value].width);
    }
    valueW = MIN(valueW, PANEL_W - 44 - keyW - 8);
    x = floor((PANEL_W - keyW - 8 - valueW) / 2);
    y = 204;
    for (NSArray *row in _rows) {
        size = [row[0] sizeWithAttributes:key];
        [row[0] drawAtPoint:NSMakePoint(x + keyW - size.width, y) withAttributes:key];
        [row[1] drawInRect:NSMakeRect(x + keyW + 8, y, valueW + 1, ROW_H) withAttributes:value];
        y += ROW_H;
    }

    // the demo's More Info button, a white pill with a hairline border
    size = [@"More Info…" sizeWithAttributes:label];
    _button = NSMakeRect(floor((PANEL_W - size.width - 28) / 2), y + 16, ceil(size.width) + 28, 22);
    [grey(0xc4) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:_button xRadius:6 yRadius:6] fill];
    [(_pressed ? (darkNow() ? grey(0x50) : grey(0xe2)) : grey(0xfa)) setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(_button, 1, 1) xRadius:5 yRadius:5] fill];
    [@"More Info…" drawAtPoint:NSMakePoint(NSMinX(_button) + 14, NSMinY(_button) + floor((22 - size.height) / 2)) withAttributes:label];

    [self drawCentered:@"© The PureDarwin Project" at:NSMaxY(_button) + 14 attributes:fine];
    [self drawCentered:@"Icons: Trenta, CC-BY-SA-4.0" at:NSMaxY(_button) + 28 attributes:fine];
}

// MenuBar runs in the background, so its panel is often not key when clicked
- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

// the title bar is gone, so the panel's whole background moves the window
- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (!NSPointInRect(p, _button)) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    _pressed = YES;
    [self display];
    _pressed = NO;
    [self setNeedsDisplay:YES];
    [self moreInfo];
}

// the full hardware report, like the System Report macOS opens from here
- (void)moreInfo
{
    [[NSWorkspace sharedWorkspace] launchApplication:@"/Applications/Utilities/System Information.app"];
}

@end

@implementation AboutPanel

+ (AboutPanel *)sharedPanel
{
    static AboutPanel *panel;

    if (panel == nil)
        panel = [[AboutPanel alloc] init];
    return panel;
}

- (void)show
{
    NSRect screen = [[NSScreen mainScreen] visibleFrame];

    if (_window == nil) {
        _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, PANEL_W, PANEL_H)
                                              styleMask:NSTitledWindowMask | NSClosableWindowMask | NSFullSizeContentViewWindowMask
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
        [_window setTitle:@"About PureDarwin"];
        [_window setTitlebarAppearsTransparent:YES];
        [_window setTitleVisibility:NSWindowTitleHidden];
        [_window setReleasedWhenClosed:NO];
        [_window setContentView:[[[AboutView alloc] initWithFrame:NSMakeRect(0, 0, PANEL_W, PANEL_H)] autorelease]];
    }
    [_window setFrameOrigin:NSMakePoint(floor(NSMidX(screen) - PANEL_W / 2), floor(NSMidY(screen) - PANEL_H / 2 + 40))];
    [_window makeKeyAndOrderFront:nil];
}

@end
