/*
 * main.m - Install PureDarwin: asks where to install, runs pdinstall and shows its progress.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import "PDWiFiUI.h"
#include <spawn.h>
#include <signal.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <dispatch/dispatch.h>

extern char **environ;

// userland installs pdinstall in /sbin, an older layout had it in /usr/sbin
static const char *pdinstallPath(void)
{
    return access("/sbin/pdinstall", X_OK) == 0 ? "/sbin/pdinstall" : "/usr/sbin/pdinstall";
}

#define WIN_W 800
#define WIN_H 560
#define SIDE_W 200
#define HEAD_H 52
#define BAR_H 54
#define PAD 22
#define MIN_FREE 8000000000ULL
#define ESP_BYTES (200ULL << 20)

static NSColor *rgba(int r, int g, int b, CGFloat a)
{
    return [NSColor colorWithCalibratedRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:a];
}

static NSColor *rgb(int r, int g, int b)
{
    return rgba(r, g, b, 1);
}

static NSFont *font(NSString *name, CGFloat size)
{
    return [NSFont fontWithName:name size:size] ?: [NSFont systemFontOfSize:size];
}

static NSDictionary *text(NSString *face, CGFloat size, NSColor *colour)
{
    return @{ NSFontAttributeName: font(face, size), NSForegroundColorAttributeName: colour };
}

static NSString *sizeString(unsigned long long bytes)
{
    double gb = bytes / 1e9;
    if (gb >= 1000)
        return [NSString stringWithFormat:NSLocalizedString(@"%.1f TB", @""), gb / 1000];
    if (gb < 1)
        return [NSString stringWithFormat:NSLocalizedString(@"%.0f MB", @""), bytes / 1e6];
    return [NSString stringWithFormat:gb < 10 ? NSLocalizedString(@"%.1f GB", @"") : NSLocalizedString(@"%.0f GB", @""), gb];
}

static NSString *sysctlString(const char *name)
{
    char buf[256];
    size_t len = sizeof(buf);
    if (sysctlbyname(name, buf, &len, NULL, 0) != 0 || len == 0)
        return nil;
    buf[sizeof(buf) - 1] = 0;
    return [NSString stringWithUTF8String:buf];
}

static unsigned long long sysctlNumber(const char *name)
{
    unsigned long long v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
        return 0;
    return len == 4 ? (unsigned long long)*(uint32_t *)&v : v;
}

// the palette from the demo, dark on the install drive, the light one kept for an installed system
typedef struct {
    NSColor *bg, *side, *line, *fg, *muted, *lead, *card, *cardLine, *cardEdge, *label, *button, *stepNow, *stepDone, *stepTodo;
} Palette;

static Palette P;

static void loadPalette(void)
{
    // the install drive has no appearance chosen yet, and with none the demo's installer is dark
    BOOL dark = YES;

    if (dark) {
        P = (Palette){ rgb(0x24, 0x22, 0x24), rgb(0x2e, 0x2b, 0x2d), rgb(0x14, 0x14, 0x14), rgb(0xe6, 0xe6, 0xe6),
                       rgb(0x9a, 0x93, 0x95), rgb(0x9a, 0x93, 0x95), rgb(0x30, 0x2d, 0x2f), rgb(0x3c, 0x38, 0x3a),
                       rgba(255, 255, 255, 0.06), rgb(0x9a, 0x8b, 0x7e), rgb(0x3a, 0x35, 0x37), rgb(0xf2, 0xee, 0xec),
                       rgb(0xbd, 0xb6, 0xb8), rgb(0x7d, 0x76, 0x78) };
    } else {
        P = (Palette){ rgb(0xf4, 0xf1, 0xee), rgb(0xe8, 0xe4, 0xe2), rgb(0xe4, 0xdd, 0xd7), rgb(0x1b, 0x1b, 0x1b),
                       rgb(0x8a, 0x8a, 0x8a), rgb(0x6e, 0x6e, 0x6e), [NSColor whiteColor], rgb(0xf0, 0xeb, 0xe6),
                       rgba(80, 50, 30, 0.08), rgb(0x9a, 0x8b, 0x7e), [NSColor whiteColor], rgb(0x1b, 0x1b, 0x1b),
                       rgb(0x4a, 0x4a, 0x4a), rgb(0x8a, 0x8a, 0x8a) };
    }
    for (NSColor **c = &P.bg; c <= &P.stepTodo; c++)
        [*c retain];
}

// the accent the default Sunset wallpaper brings
static NSColor *accent(void)
{
    return rgb(0xcf, 0x5f, 0x3c);
}

typedef enum { StepIntro, StepLicence, StepNetwork, StepDestination, StepType, StepInstall, StepDone, StepCount } Step;

// translated where drawn, a static initialiser cannot call NSLocalizedString
static NSString *const stepNames[StepCount] = { @"Introduction", @"Licence", @"Network", @"Destination", @"Installation Type",
                                                @"Installation", @"Summary" };

// one slice of a disk bar: kind is esp, keep, pd or free
@interface Slice : NSObject
@property (copy) NSString *kind;
@property (copy) NSString *label;
@property unsigned long long start;
@property unsigned long long bytes;
@end

@implementation Slice
+ (instancetype)kind:(NSString *)kind label:(NSString *)label start:(unsigned long long)start bytes:(unsigned long long)bytes
{
    Slice *s = [[[Slice alloc] init] autorelease];
    s.kind = kind;
    s.label = label;
    s.start = start;
    s.bytes = bytes;
    return s;
}
- (void)dealloc
{
    [_kind release];
    [_label release];
    [super dealloc];
}
@end

// a disk pdinstall can install onto, its partitions and largest free gap in bytes
@interface Disk : NSObject
@property (copy) NSString *name;
@property unsigned long long size;
@property unsigned long long freeStart;
@property unsigned long long freeBytes;
@property BOOL boot;
@property BOOL removable;
@property (retain) NSMutableArray *parts;
@end

@implementation Disk
- (instancetype)init
{
    if ((self = [super init]) != nil)
        _parts = [[NSMutableArray alloc] init];
    return self;
}
- (void)dealloc
{
    [_name release];
    [_parts release];
    [super dealloc];
}
- (BOOL)hasEsp
{
    for (Slice *s in _parts) {
        if ([s.kind isEqualToString:@"esp"])
            return YES;
    }
    return NO;
}
- (BOOL)canUseFree
{
    return _freeBytes >= MIN_FREE + ([self hasEsp] ? 0 : ESP_BYTES) && [_parts count] > 0;
}
// what is on the disk now, by position, with the largest free gap
- (NSArray *)slices
{
    NSMutableArray *a = [NSMutableArray arrayWithArray:_parts];
    if (_freeBytes)
        [a addObject:[Slice kind:@"free" label:NSLocalizedString(@"Free", @"") start:_freeStart bytes:_freeBytes]];
    [a sortUsingComparator:^NSComparisonResult(Slice *x, Slice *y) {
        return x.start < y.start ? NSOrderedAscending : x.start > y.start ? NSOrderedDescending : NSOrderedSame;
    }];
    return a;
}
@end

// a name for a partition from its gpt type
static Slice *partSlice(NSString *type, unsigned long long start, unsigned long long bytes)
{
    if ([type rangeOfString:@"EFI System"].location != NSNotFound)
        return [Slice kind:@"esp" label:@"EFI" start:start bytes:bytes];
    NSString *label = NSLocalizedString(@"Partition", @"");
    if ([type rangeOfString:@"Microsoft"].location != NSNotFound || [type rangeOfString:@"Windows"].location != NSNotFound)
        label = @"Windows";
    else if ([type rangeOfString:@"Linux"].location != NSNotFound)
        label = @"Linux";
    else if ([type rangeOfString:@"Apple"].location != NSNotFound)
        label = @"macOS";
    return [Slice kind:@"keep" label:label start:start bytes:bytes];
}

// a clickable part of a drawn step
@interface Hit : NSObject {
@public
    NSRect rect;
    void (^action)(void);
}
@end

@implementation Hit
- (void)dealloc
{
    [action release];
    [super dealloc];
}
@end

@interface InstallerView : NSView {
    NSMutableArray *_hits;
    Step _step;
    BOOL _agreed;
    BOOL _sheet;
    NSMutableArray *_disks;
    NSInteger _selected;
    BOOL _useFree;

    NSString *_cpu, *_memory, *_version, *_facts;
    NSImage *_logo, *_mark, *_footLogo;
    NSMutableArray *_nets;

    NSRect _goRect, _backRect, _agreeRect, _disagreeRect, _detailsRect;
    NSRect _diskRects[8];
    NSRect _segRects[2];

    pid_t _child;
    dispatch_source_t _source;
    NSMutableString *_pending;
    NSMutableArray *_log;
    NSString *_now;
    NSString *_error;
    NSString *_result;
    double _progress;
    BOOL _details;
}
@end

@implementation InstallerView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _disks = [[NSMutableArray alloc] init];
    _nets = [[NSMutableArray alloc] init];
    _hits = [[NSMutableArray alloc] init];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(wifiChanged:) name:PDWiFiChangedNotification object:nil];
    _log = [[NSMutableArray alloc] init];
    _pending = [[NSMutableString alloc] init];
    _selected = -1;
    _details = YES;
    [self loadMachine];
    [self loadNetworks];
    _logo = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"beak-white" ofType:@"png"]];
    _mark = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"beak-mark" ofType:@"png"]];
    _footLogo = [[NSImage alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"beak-grey" ofType:@"png"]];
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

// what the introduction says about this computer and the system on the install drive
- (void)loadMachine
{
    NSString *brand = sysctlString("machdep.cpu.brand_string");
    unsigned long long cores = sysctlNumber("hw.physicalcpu");
    unsigned long long mem = sysctlNumber("hw.memsize");
    NSString *kver = sysctlString("kern.version") ?: @"";
    NSRange x = [kver rangeOfString:@"xnu-"];
    NSString *xnu = @"xnu";
    NSDictionary *sv = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];

    if (x.location != NSNotFound) {
        NSString *rest = [kver substringFromIndex:x.location];
        NSRange end = [rest rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"~/ "]];
        xnu = end.location != NSNotFound ? [rest substringToIndex:end.location] : rest;
    }
    brand = [brand stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    _cpu = [[NSString stringWithFormat:@"%@%@", brand ?: NSLocalizedString(@"Unknown processor", @""),
             cores ? [NSString stringWithFormat:NSLocalizedString(@", %llu cores", @""), cores] : @""] retain];
    _memory = [(mem ? [NSString stringWithFormat:NSLocalizedString(@"%.0f GB", @""), mem / 1073741824.0] : NSLocalizedString(@"Unknown", @"")) retain];
    _version = [[NSString stringWithFormat:NSLocalizedString(@"PureDarwin %@", @""), sv[@"ProductVersion"] ?: @""] retain];
    _facts = [[NSString stringWithFormat:NSLocalizedString(@"Darwin %1$@ · %2$@ · build %3$@", @""), sysctlString("kern.osrelease") ?: @"",
               xnu, sysctlString("kern.osversion") ?: @""] retain];
}

// the network interfaces and their IPv4 addresses, loopback left out
- (void)loadNetworks
{
    struct ifaddrs *ifs = NULL, *i;
    NSMutableDictionary *byName = [NSMutableDictionary dictionary];
    NSMutableArray *order = [NSMutableArray array];

    [_nets removeAllObjects];
    if (getifaddrs(&ifs) != 0)
        return;
    for (i = ifs; i != NULL; i = i->ifa_next) {
        if ((i->ifa_flags & IFF_LOOPBACK) || i->ifa_name == NULL)
            continue;
        NSString *name = [NSString stringWithUTF8String:i->ifa_name];
        if ([name hasPrefix:@"gif"] || [name hasPrefix:@"stf"] || [name hasPrefix:@"utun"] || [name hasPrefix:@"bridge"])
            continue;
        if (byName[name] == nil) {
            byName[name] = @"";
            [order addObject:name];
        }
        if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && (i->ifa_flags & IFF_RUNNING)) {
            char a[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, a, sizeof(a));
            byName[name] = [NSString stringWithUTF8String:a];
        }
    }
    freeifaddrs(ifs);
    for (NSString *n in order)
        [_nets addObject:@[ n, byName[n] ]];
}

- (void)wifiChanged:(NSNotification *)note
{
    [self loadNetworks];
    [self setNeedsDisplay:YES];
}

- (void)hit:(NSRect)r action:(void (^)(void))action
{
    Hit *h = [[[Hit alloc] init] autorelease];
    h->rect = r;
    h->action = [action copy];
    [_hits addObject:h];
}

- (BOOL)connected
{
    if ([PDWiFi shared].current != nil)
        return YES;
    for (NSArray *n in _nets) {
        if ([n[1] length] > 0)
            return YES;
    }
    return NO;
}

// pdinstall disks: "disk NAME BYTES [boot] [removable]", "part NAMEsN START BLOCKS INDEX GPT part - TYPE", "free NAME START BYTES"
- (void)loadDisks
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s disks 2>/dev/null", pdinstallPath());
    FILE *f = popen(cmd, "r");
    char line[512];
    Disk *cur = nil;

    [_disks removeAllObjects];
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        NSString *l = [[NSString stringWithUTF8String:line] stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
        NSArray *w = [l componentsSeparatedByString:@" "];
        if ([w count] >= 3 && [w[0] isEqualToString:@"disk"]) {
            cur = [[[Disk alloc] init] autorelease];
            cur.name = w[1];
            cur.size = strtoull([w[2] UTF8String], NULL, 10);
            // the medium this runs from, or one that cannot be written such as a CD
            cur.boot = [w containsObject:@"boot"] || [w containsObject:@"readonly"];
            cur.removable = [w containsObject:@"removable"];
            [_disks addObject:cur];
        } else if ([w count] >= 4 && [w[0] isEqualToString:@"free"] && cur != nil) {
            cur.freeStart = strtoull([w[2] UTF8String], NULL, 10) * 512;
            cur.freeBytes = strtoull([w[3] UTF8String], NULL, 10);
        } else if ([w count] >= 5 && [w[0] isEqualToString:@"part"] && cur != nil) {
            NSRange r = [l rangeOfString:@"GPT part - "];
            NSString *type = r.location != NSNotFound ? [l substringFromIndex:NSMaxRange(r)] : @"";
            [cur.parts addObject:partSlice(type, strtoull([w[2] UTF8String], NULL, 10) * 512,
                                           strtoull([w[3] UTF8String], NULL, 10) * 512)];
        }
    }
    if (f != NULL)
        pclose(f);
    _selected = -1;
    for (NSUInteger i = 0; i < [_disks count]; i++) {
        if (![_disks[i] boot]) {
            _selected = (NSInteger)i;
            break;
        }
    }
    [self chooseMode];
}

// free space when there is room for it, otherwise the whole disk
- (void)chooseMode
{
    _useFree = [[self selectedDisk] canUseFree];
}

- (Disk *)selectedDisk
{
    return _selected >= 0 && _selected < (NSInteger)[_disks count] ? _disks[_selected] : nil;
}

- (BOOL)destinationOK
{
    Disk *d = [self selectedDisk];
    return d != nil && !d.boot && (!_useFree || [d canUseFree]);
}

// the selected disk as it will be afterwards
- (NSArray *)plannedSlices
{
    Disk *d = [self selectedDisk];
    if (!_useFree)
        return @[ [Slice kind:@"esp" label:@"EFI" start:0 bytes:ESP_BYTES], [Slice kind:@"pd" label:@"PureDarwin" start:ESP_BYTES
                                                                                 bytes:d.size - ESP_BYTES] ];
    NSMutableArray *a = [NSMutableArray array];
    for (Slice *s in [d slices]) {
        if (![s.kind isEqualToString:@"free"]) {
            [a addObject:s];
            continue;
        }
        unsigned long long pd = s.bytes;
        if (![d hasEsp]) {
            [a addObject:[Slice kind:@"esp" label:@"EFI" start:s.start bytes:ESP_BYTES]];
            pd -= ESP_BYTES;
        }
        [a addObject:[Slice kind:@"pd" label:@"PureDarwin" start:s.start bytes:pd]];
    }
    return a;
}

- (unsigned long long)plannedBytes
{
    for (Slice *s in [self plannedSlices]) {
        if ([s.kind isEqualToString:@"pd"])
            return s.bytes;
    }
    return 0;
}

// drawing helpers

- (CGFloat)contentX
{
    return SIDE_W + PAD;
}

- (CGFloat)contentW
{
    return WIN_W - SIDE_W - 2 * PAD;
}

- (void)fillRound:(NSRect)r radius:(CGFloat)rad colour:(NSColor *)c
{
    [c setFill];
    [[NSBezierPath bezierPathWithRoundedRect:r xRadius:rad yRadius:rad] fill];
}

// words wrapped to the width by hand, drawn line by line
- (CGFloat)wrap:(NSString *)s x:(CGFloat)x y:(CGFloat)y width:(CGFloat)w attrs:(NSDictionary *)a lineHeight:(CGFloat)lh
{
    NSMutableString *line = [NSMutableString string];
    for (NSString *word in [s componentsSeparatedByString:@" "]) {
        NSString *next = [line length] ? [NSString stringWithFormat:@"%@ %@", line, word] : word;
        if ([line length] && [next sizeWithAttributes:a].width > w) {
            [line drawAtPoint:NSMakePoint(x, y) withAttributes:a];
            y += lh;
            [line setString:word];
        } else {
            [line setString:next];
        }
    }
    if ([line length]) {
        [line drawAtPoint:NSMakePoint(x, y) withAttributes:a];
        y += lh;
    }
    return y;
}

- (CGFloat)lead:(NSString *)s at:(CGFloat)y
{
    return [self wrap:s x:[self contentX] y:y width:MIN([self contentW], 520) attrs:text(@"Inter-Regular", 12.5, P.lead) lineHeight:19] + 14;
}

- (CGFloat)cardTitle:(NSString *)s at:(CGFloat)y
{
    NSMutableDictionary *a = [[text(@"Inter-SemiBold", 11, P.label) mutableCopy] autorelease];
    a[NSKernAttributeName] = @0.6;
    [[s uppercaseString] drawAtPoint:NSMakePoint([self contentX] + 2, y) withAttributes:a];
    return y + 20;
}

- (void)card:(NSRect)r
{
    [self fillRound:NSInsetRect(r, -1, -1) radius:10 colour:P.cardEdge];
    [self fillRound:r radius:9 colour:P.card];
}

// one card row: a title with a smaller line under it, and a value or status on the right
- (void)row:(NSRect)r title:(NSString *)title sub:(NSString *)sub divider:(BOOL)divider
{
    if (divider) {
        [P.cardLine setFill];
        NSRectFill(NSMakeRect(NSMinX(r), NSMinY(r), NSWidth(r), 1));
    }
    CGFloat ty = sub ? NSMinY(r) + 8 : NSMidY(r) - 8;
    [title drawAtPoint:NSMakePoint(NSMinX(r) + 12, ty) withAttributes:text(@"Inter-Regular", 13, P.fg)];
    if (sub)
        [sub drawAtPoint:NSMakePoint(NSMinX(r) + 12, ty + 18) withAttributes:text(@"Inter-Regular", 11, P.muted)];
}

- (void)status:(NSString *)s ok:(BOOL)ok in:(NSRect)r
{
    NSDictionary *a = text(@"Inter-Regular", 12, P.lead);
    NSSize sz = [s sizeWithAttributes:a];
    CGFloat x = NSMaxX(r) - 12 - sz.width;
    [(ok ? rgb(0x4c, 0xae, 0x4f) : rgb(0xf2, 0xa3, 0x3a)) setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x - 13, NSMidY(r) - 3.5, 7, 7)] fill];
    [s drawAtPoint:NSMakePoint(x, NSMidY(r) - sz.height / 2) withAttributes:a];
}

- (void)valueText:(NSString *)s in:(NSRect)r
{
    NSDictionary *a = text(@"Inter-Regular", 13, P.lead);
    NSSize sz = [s sizeWithAttributes:a];
    [s drawAtPoint:NSMakePoint(NSMaxX(r) - 12 - sz.width, NSMidY(r) - sz.height / 2) withAttributes:a];
}

// a segmented control, right-aligned in the row, its parts' rects kept for clicks
- (void)segment:(NSArray *)labels on:(NSInteger)on enabled:(NSArray *)enabled in:(NSRect)r rects:(NSRect *)rects
{
    NSDictionary *a = text(@"Inter-Regular", 12, P.fg);
    CGFloat w = 4;
    for (NSString *l in labels)
        w += [l sizeWithAttributes:a].width + 24;
    NSRect box = NSMakeRect(NSMaxX(r) - 12 - w, NSMidY(r) - 12, w, 24);
    [self fillRound:box radius:7 colour:P.side];
    CGFloat x = NSMinX(box) + 2;
    for (NSUInteger i = 0; i < [labels count]; i++) {
        CGFloat lw = [labels[i] sizeWithAttributes:a].width + 24;
        NSRect part = NSMakeRect(x, NSMinY(box) + 2, lw, 20);
        if ((NSInteger)i == on)
            [self fillRound:part radius:5 colour:P.card];
        BOOL en = enabled == nil || [enabled[i] boolValue];
        NSDictionary *la = text(@"Inter-Regular", 12, en ? P.fg : P.muted);
        [labels[i] drawAtPoint:NSMakePoint(x + 12, NSMinY(part) + 2) withAttributes:la];
        if (rects)
            rects[i] = part;
        x += lw;
    }
}

- (NSColor *)sliceColour:(NSString *)kind
{
    if ([kind isEqualToString:@"esp"])
        return rgb(0x9a, 0x94, 0x8e);
    if ([kind isEqualToString:@"keep"])
        return rgb(0x7f, 0x97, 0xbd);
    if ([kind isEqualToString:@"pd"])
        return accent();
    // opaque, NSRectFill copies alpha instead of blending it
    return P.side;
}

// the disk drawn to scale, a label in each slice wide enough for it
- (void)diskBar:(NSArray *)slices in:(NSRect)r
{
    unsigned long long total = 0;
    for (Slice *s in slices)
        total += s.bytes;
    if (total == 0) {
        [self fillRound:r radius:6 colour:[self sliceColour:@"free"]];
        [NSLocalizedString(@"Empty", @"") drawAtPoint:NSMakePoint(NSMidX(r) - 16, NSMinY(r) + 6) withAttributes:text(@"Inter-SemiBold", 10.5, P.muted)];
        return;
    }
    [NSGraphicsContext saveGraphicsState];
    [[NSBezierPath bezierPathWithRoundedRect:r xRadius:6 yRadius:6] addClip];
    CGFloat x = NSMinX(r);
    for (NSUInteger i = 0; i < [slices count]; i++) {
        Slice *s = slices[i];
        CGFloat w = i + 1 == [slices count] ? NSMaxX(r) - x : MAX(3, round(NSWidth(r) * s.bytes / total));
        NSRect part = NSMakeRect(x, NSMinY(r), w, NSHeight(r));
        [[self sliceColour:s.kind] setFill];
        NSRectFill(part);
        if (i > 0) {
            [P.card setFill];
            NSRectFill(NSMakeRect(x, NSMinY(r), 1, NSHeight(r)));
        }
        BOOL free = [s.kind isEqualToString:@"free"];
        NSDictionary *a = text(@"Inter-SemiBold", 10.5, free ? P.muted : [NSColor whiteColor]);
        NSString *l = [NSString stringWithFormat:@"%@, %@", s.label, sizeString(s.bytes)];
        NSSize sz = [l sizeWithAttributes:a];
        if (sz.width + 12 > w) {
            l = s.label;
            sz = [l sizeWithAttributes:a];
        }
        if (sz.width + 12 <= w)
            [l drawAtPoint:NSMakePoint(floor(NSMidX(part) - sz.width / 2), floor(NSMidY(part) - sz.height / 2)) withAttributes:a];
        x += w;
    }
    [NSGraphicsContext restoreGraphicsState];
}

// the key under a disk bar
- (void)barKey:(NSArray *)items y:(CGFloat)y x:(CGFloat)x
{
    NSDictionary *a = text(@"Inter-Regular", 11, P.muted);
    for (NSArray *it in items) {
        [self fillRound:NSMakeRect(x, y + 4, 8, 8) radius:2 colour:it[1]];
        [it[0] drawAtPoint:NSMakePoint(x + 13, y) withAttributes:a];
        x += [it[0] sizeWithAttributes:a].width + 30;
    }
}

// a drive drawn as a drive: body, a light, a connector on a removable one
- (void)drive:(BOOL)removable centre:(NSPoint)c dim:(BOOL)dim
{
    NSColor *top = rgb(0xe9, 0xe6, 0xe3), *bottom = rgb(0xc9, 0xc4, 0xbf);
    NSRect body = removable ? NSMakeRect(c.x - 15, c.y - 18, 30, 42) : NSMakeRect(c.x - 27, c.y - 20, 54, 40);
    if (dim) {
        top = [top colorWithAlphaComponent:0.45];
        bottom = [bottom colorWithAlphaComponent:0.45];
    }
    if (removable)
        [self fillRound:NSMakeRect(c.x - 8, NSMinY(body) - 8, 16, 10) radius:2 colour:rgb(0xb9, 0xb4, 0xaf)];
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:body xRadius:7 yRadius:7];
    NSGradient *g = [[NSGradient alloc] initWithStartingColor:top endingColor:bottom];
    [g drawInBezierPath:p angle:90];
    [g release];
    [rgba(0, 0, 0, 0.18) setStroke];
    [p setLineWidth:1];
    [p stroke];
    [rgb(0x4c, 0xae, 0x4f) setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMaxX(body) - 13, NSMaxY(body) - 12, 5, 5)] fill];
}

- (NSRect)button:(NSString *)title rightEdge:(CGFloat)right primary:(BOOL)primary enabled:(BOOL)enabled
{
    NSDictionary *a = text(@"Inter-Regular", 13, primary ? [NSColor whiteColor] : P.fg);
    NSSize sz = [title sizeWithAttributes:a];
    CGFloat w = MAX(92, sz.width + 32);
    NSRect r = NSMakeRect(right - w, WIN_H - BAR_H + 13, w, 28);
    NSColor *fill = primary ? accent() : P.button;
    if (!enabled)
        fill = [fill colorWithAlphaComponent:0.45];
    if (!primary)
        [self fillRound:NSInsetRect(r, -1, -1) radius:7 colour:rgba(0, 0, 0, 0.15)];
    [self fillRound:r radius:6 colour:fill];
    if (!enabled)
        a = text(@"Inter-Regular", 13, [a[NSForegroundColorAttributeName] colorWithAlphaComponent:0.6]);
    [title drawAtPoint:NSMakePoint(floor(NSMidX(r) - sz.width / 2), floor(NSMidY(r) - sz.height / 2)) withAttributes:a];
    return r;
}

// the steps

- (void)drawIntro:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW];
    NSRect banner = NSMakeRect(x, y, w, 96);
    NSBezierPath *p = [NSBezierPath bezierPathWithRoundedRect:banner xRadius:10 yRadius:10];
    NSGradient *g = [[NSGradient alloc] initWithStartingColor:rgb(0xc9, 0x7b, 0x50) endingColor:rgb(0xb8, 0x45, 0x46)];
    [g drawInBezierPath:p angle:-20];
    [g release];
    // the beak large and faint off the right edge, then the beak beside the release
    [NSGraphicsContext saveGraphicsState];
    [p addClip];
    [_mark drawInRect:NSMakeRect(NSMaxX(banner) - 170, y - 50, 200, 200) fromRect:NSZeroRect operation:NSCompositeSourceOver
             fraction:0.14 respectFlipped:YES hints:nil];
    [NSGraphicsContext restoreGraphicsState];
    [_logo drawInRect:NSMakeRect(x + 24, y + 20, 56, 56) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1
       respectFlipped:YES hints:nil];
    CGFloat tx = x + 100;
    [_version drawAtPoint:NSMakePoint(tx, y + 18) withAttributes:text(@"Inter-Bold", 22, [NSColor whiteColor])];
    [_facts drawAtPoint:NSMakePoint(tx, y + 48) withAttributes:text(@"Inter-Regular", 12, rgba(255, 255, 255, 0.9))];
    NSDictionary *pa = text(@"Inter-Regular", 11.5, [NSColor whiteColor]);
    NSString *pill = NSLocalizedString(@"from the install drive", @"");
    NSSize ps = [pill sizeWithAttributes:pa];
    [self fillRound:NSMakeRect(tx, y + 68, ps.width + 18, 20) radius:10 colour:rgba(255, 255, 255, 0.18)];
    [pill drawAtPoint:NSMakePoint(tx + 9, y + 70) withAttributes:pa];
    y += 114;

    y = [self lead:NSLocalizedString(@"This puts PureDarwin on a disk in this computer. You'll pick the disk, then it takes a few minutes. Nothing "
                   @"changes until you choose Install.", @"") at:y];
    y = [self cardTitle:NSLocalizedString(@"This computer", @"") at:y];
    NSMutableArray *names = [NSMutableArray array];
    for (NSArray *n in _nets)
        [names addObject:n[0]];
    NSString *net = [names count] ? [names componentsJoinedByString:@", "] : NSLocalizedString(@"No network hardware found", @"");
    NSString *rows[][2] = { { NSLocalizedString(@"Processor", @""), _cpu }, { NSLocalizedString(@"Memory", @""), _memory }, { NSLocalizedString(@"Firmware", @""), @"UEFI" },
                            { NSLocalizedString(@"Graphics", @""), NSLocalizedString(@"The firmware's framebuffer, basic display only", @"") }, { NSLocalizedString(@"Network", @""), net } };
    BOOL ok[] = { YES, YES, YES, NO, [names count] > 0 };
    [self card:NSMakeRect(x, y, w, 5 * 41)];
    for (int i = 0; i < 5; i++) {
        NSRect r = NSMakeRect(x, y + i * 41, w, 41);
        [self row:r title:rows[i][0] sub:rows[i][1] divider:i > 0];
        [self status:ok[i] ? NSLocalizedString(@"Supported", @"") : i == 3 ? NSLocalizedString(@"Partly supported", @"") : NSLocalizedString(@"Not found", @"") ok:ok[i] in:r];
    }
}

- (void)drawLicence:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW];
    y = [self lead:NSLocalizedString(@"PureDarwin is free software. Read how you can use and share it.", @"") at:y];
    struct { NSString *head, *body; } parts[] = {
        { @"Apple Public Source License 2.0", NSLocalizedString(@"The kernel, launchd and most of the core system are Apple's open source releases "
          @"under the APSL 2.0. You may use, study, change and share them. If you share a changed version, its source has "
          @"to be available too.", @"") },
        { NSLocalizedString(@"Other components", @""), NSLocalizedString(@"Parts of the system come from other projects under their own licences, among them BSD, "
          @"MIT, LGPL and GPL. Each component's licence ships with it.", @"") },
        { NSLocalizedString(@"Vendor firmware", @""), NSLocalizedString(@"Some devices only work with firmware supplied by their vendor. PureDarwin includes it only "
          @"where the vendor allows it to be shared, under the vendor's own terms.", @"") },
        { NSLocalizedString(@"No warranty", @""), NSLocalizedString(@"PureDarwin comes as is, without warranty of any kind. Back up anything you care about before "
          @"installing.", @"") },
    };
    [self card:NSMakeRect(x, y, w, WIN_H - BAR_H - 16 - y)];
    CGFloat ty = y + 14;
    for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
        [parts[i].head drawAtPoint:NSMakePoint(x + 16, ty) withAttributes:text(@"Inter-SemiBold", 12, P.fg)];
        ty = [self wrap:parts[i].body x:x + 16 y:ty + 18 width:w - 32 attrs:text(@"Inter-Regular", 12, P.lead) lineHeight:18] + 10;
    }
}

// a small button at the right of a row, its left edge returned for the next one
- (CGFloat)mini:(NSString *)title right:(CGFloat)right mid:(CGFloat)mid action:(void (^)(void))action
{
    NSDictionary *a = text(@"Inter-Regular", 12, P.fg);
    NSSize sz = [title sizeWithAttributes:a];
    NSRect b = NSMakeRect(right - sz.width - 20, mid - 11, sz.width + 20, 22);

    [self fillRound:NSInsetRect(b, -0.5, -0.5) radius:6.5 colour:rgba(0, 0, 0, 0.2)];
    [self fillRound:b radius:6 colour:P.button];
    [title drawAtPoint:NSMakePoint(NSMinX(b) + 10, mid - sz.height / 2) withAttributes:a];
    [self hit:NSInsetRect(b, -3, -4) action:action];
    return NSMinX(b) - 10;
}

- (void)wifiRow:(NSRect)r name:(NSString *)name sub:(NSString *)sub level:(int)level on:(BOOL)on lock:(BOOL)lock divider:(BOOL)div
{
    NSRect circle = NSMakeRect(NSMinX(r) + 12, NSMidY(r) - 12, 24, 24);

    if (div) {
        [P.cardLine setFill];
        NSRectFill(NSMakeRect(NSMinX(r), NSMinY(r), NSWidth(r), 1));
    }
    [self fillRound:circle radius:12 colour:on ? accent() : rgba(127, 127, 127, 0.18)];
    PDDrawWiFiGlyph(NSInsetRect(circle, 5, 5), level, on ? [NSColor whiteColor] : P.fg, YES);
    NSDictionary *na = text(@"Inter-Regular", 13, P.fg);
    CGFloat ty = sub ? NSMinY(r) + 7 : NSMidY(r) - 8;
    [name drawAtPoint:NSMakePoint(NSMinX(r) + 46, ty) withAttributes:na];
    if (lock)
        PDDrawWiFiLock(NSMakeRect(NSMinX(r) + 52 + [name sizeWithAttributes:na].width, ty + 2, 9, 11), P.muted, YES);
    if (sub)
        [sub drawAtPoint:NSMakePoint(NSMinX(r) + 46, ty + 18) withAttributes:text(@"Inter-Regular", 11, P.muted)];
}

// the demo's network step: Ethernet, then Wi-Fi with the networks in range
- (void)drawNetwork:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW], bottom = WIN_H - BAR_H - 12;
    PDWiFi *wifi = [PDWiFi shared];
    PDWiFiNetwork *cur = wifi.current;
    NSMutableArray *wired = [NSMutableArray array];

    y = [self lead:NSLocalizedString(@"The install doesn't need a network, everything comes from the install drive. Connecting now carries the "
                   @"network over, so the installed system joins it on first start.", @"") at:y];
    for (NSArray *n in _nets) {
        if (!wifi.interface || ![n[0] isEqualToString:wifi.interface])
            [wired addObject:n];
    }
    y = [self cardTitle:NSLocalizedString(@"Ethernet", @"") at:y];
    NSUInteger en = MAX([wired count], 1);
    [self card:NSMakeRect(x, y, w, en * 40)];
    if ([wired count] == 0)
        [self row:NSMakeRect(x, y, w, 40) title:NSLocalizedString(@"No cable connected", @"") sub:nil divider:NO];
    for (NSUInteger i = 0; i < [wired count]; i++) {
        NSRect r = NSMakeRect(x, y + i * 40, w, 40);
        BOOL up = [wired[i][1] length] > 0;
        [self row:r title:wired[i][0] sub:nil divider:i > 0];
        [self status:up ? [NSString stringWithFormat:NSLocalizedString(@"Connected, %@", @""), wired[i][1]] : NSLocalizedString(@"No cable connected", @"") ok:up in:r];
    }
    y += en * 40 + 18;

    y = [self cardTitle:@"Wi-Fi" at:y];
    if (!wifi.hardware) {
        [self card:NSMakeRect(x, y, w, 40)];
        [self row:NSMakeRect(x, y, w, 40) title:NSLocalizedString(@"No Wi-Fi hardware found", @"") sub:nil divider:NO];
        return;
    }
    // the switch, the joined network, then as many others as fit, then Other Network
    NSMutableArray *others = [NSMutableArray array];
    for (PDWiFiNetwork *n in wifi.inRange) {
        if (!cur || ![cur.ssid isEqualToString:n.ssid])
            [others addObject:n];
    }
    CGFloat fixedH = 44 + (cur ? 44 : 0) + (wifi.powered ? 36 : 0);
    NSUInteger fit = wifi.powered ? (NSUInteger)MAX(0, (bottom - y - fixedH) / 38) : 0;
    NSUInteger shown = MIN([others count], fit);
    BOOL looking = wifi.powered && shown == 0;
    CGFloat h = fixedH + (shown + (looking && fit > 0 ? 1 : 0)) * 38;
    [self card:NSMakeRect(x, y, w, h)];

    NSRect r = NSMakeRect(x, y, w, 44);
    NSString *state = !wifi.powered ? NSLocalizedString(@"Off", @"") : cur ? [NSString stringWithFormat:NSLocalizedString(@"Connected to %@", @""), cur.ssid]
                    : wifi.busy ? [NSString stringWithFormat:NSLocalizedString(@"Joining %@…", @""), wifi.busy] : wifi.lastError ?: NSLocalizedString(@"Not connected", @"");
    [self row:r title:@"Wi-Fi" sub:state divider:NO];
    NSRect sw = NSMakeRect(NSMaxX(r) - 12 - 32, NSMidY(r) - 9.5, 32, 19);
    [self fillRound:sw radius:9.5 colour:wifi.powered ? accent() : rgba(127, 127, 127, 0.35)];
    [[NSColor whiteColor] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(wifi.powered ? NSMaxX(sw) - 17 : NSMinX(sw) + 2, NSMinY(sw) + 2, 15, 15)] fill];
    [self hit:NSInsetRect(sw, -4, -6) action:^{ [wifi setPower:!wifi.powered]; }];
    y += 44;

    if (cur) {
        r = NSMakeRect(x, y, w, 44);
        [self wifiRow:r name:cur.ssid sub:cur.security ? [NSString stringWithFormat:NSLocalizedString(@"Connected · %@ Personal", @""), cur.security] : NSLocalizedString(@"Connected", @"")
                level:[cur bars] on:YES lock:NO divider:YES];
        [self mini:NSLocalizedString(@"Disconnect", @"") right:NSMaxX(r) - 12 mid:NSMidY(r) action:^{ [wifi leave]; }];
        y += 44;
    }
    if (!wifi.powered)
        return;
    for (NSUInteger i = 0; i < shown; i++) {
        PDWiFiNetwork *n = others[i];
        r = NSMakeRect(x, y, w, 38);
        [self wifiRow:r name:n.ssid sub:nil level:[n bars] on:NO lock:n.security != nil divider:YES];
        if ([wifi.busy isEqualToString:n.ssid]) {
            PDDrawWiFiSpinner(NSMakeRect(NSMaxX(r) - 28, NSMidY(r) - 7, 14, 14), accent());
            [self setNeedsDisplay:YES];
        } else {
            [self mini:NSLocalizedString(@"Join", @"") right:NSMaxX(r) - 12 mid:NSMidY(r) action:^{ [wifi pick:n accent:accent()]; }];
        }
        y += 38;
    }
    if (looking && fit > 0) {
        [self row:NSMakeRect(x, y, w, 38) title:wifi.scanning ? NSLocalizedString(@"Looking for networks…", @"") : NSLocalizedString(@"No networks in range", @"") sub:nil divider:YES];
        y += 38;
    }
    r = NSMakeRect(x, y, w, 36);
    [P.cardLine setFill];
    NSRectFill(NSMakeRect(x, y, w, 1));
    NSDictionary *la = text(@"Inter-Regular", 12, accent());
    NSString *l = NSLocalizedString(@"Other Network…", @"");
    [l drawAtPoint:NSMakePoint(x + 12, NSMidY(r) - 8) withAttributes:la];
    [self hit:NSMakeRect(x + 8, NSMinY(r) + 4, [l sizeWithAttributes:la].width + 8, 28) action:^{ [wifi joinOther:accent()]; }];
}

- (void)drawDestination:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW];
    y = [self lead:NSLocalizedString(@"Choose the disk to install on. The bar shows what it holds now.", @"") at:y];
    NSUInteger count = MIN([_disks count], 6);
    CGFloat tw = floor((w - 2 * 10) / 3);
    for (NSUInteger i = 0; i < count; i++) {
        Disk *d = _disks[i];
        NSRect r = NSMakeRect(x + (i % 3) * (tw + 10), y + (i / 3) * 112, tw, 102);
        _diskRects[i] = r;
        if ((NSInteger)i == _selected)
            [self fillRound:NSInsetRect(r, -2, -2) radius:11 colour:accent()];
        [self card:r];
        [self drive:d.removable centre:NSMakePoint(NSMidX(r), NSMinY(r) + 34) dim:d.boot];
        NSColor *fg = d.boot ? P.muted : P.fg;
        NSDictionary *na = text(@"Inter-SemiBold", 12.5, fg), *sa = text(@"Inter-Regular", 11, P.muted);
        NSString *name = d.boot ? NSLocalizedString(@"Install drive", @"") : d.removable ? NSLocalizedString(@"External disk", @"") : NSLocalizedString(@"Internal disk", @"");
        NSString *sub = [NSString stringWithFormat:@"%@ · %@", sizeString(d.size), d.name];
        [name drawAtPoint:NSMakePoint(floor(NSMidX(r) - [name sizeWithAttributes:na].width / 2), NSMinY(r) + 64) withAttributes:na];
        [sub drawAtPoint:NSMakePoint(floor(NSMidX(r) - [sub sizeWithAttributes:sa].width / 2), NSMinY(r) + 82) withAttributes:sa];
    }
    if (count == 0) {
        [NSLocalizedString(@"No disks found.", @"") drawAtPoint:NSMakePoint(x, y) withAttributes:text(@"Inter-Regular", 12.5, P.muted)];
        return;
    }
    y += ((count + 2) / 3) * 112 + 6;

    Disk *d = [self selectedDisk];
    if (d == nil)
        return;
    [self card:NSMakeRect(x, y, w, 96)];
    [self diskBar:[d slices] in:NSMakeRect(x + 12, y + 12, w - 24, 26)];
    [self barKey:@[ @[ @"EFI", [self sliceColour:@"esp"] ], @[ NSLocalizedString(@"In use", @""), [self sliceColour:@"keep"] ],
                    @[ [NSString stringWithFormat:NSLocalizedString(@"Free, %@", @""), sizeString(d.freeBytes)], rgb(0xb9, 0xb4, 0xaf) ] ]
               y:y + 44 x:x + 12];
    NSRect r = NSMakeRect(x, y + 60, w, 36);
    NSString *sub = d.boot ? NSLocalizedString(@"PureDarwin is running from this drive", @"")
                  : _useFree ? NSLocalizedString(@"What's on the disk stays where it is", @"") : NSLocalizedString(@"Everything on the disk is deleted", @"");
    [P.cardLine setFill];
    NSRectFill(NSMakeRect(x, NSMinY(r), w, 1));
    [NSLocalizedString(@"Use", @"") drawAtPoint:NSMakePoint(x + 12, NSMinY(r) + 2) withAttributes:text(@"Inter-Regular", 13, P.fg)];
    [sub drawAtPoint:NSMakePoint(x + 12, NSMinY(r) + 19) withAttributes:text(@"Inter-Regular", 11, P.muted)];
    [self segment:@[ NSLocalizedString(@"Free Space", @""), NSLocalizedString(@"Whole Disk", @"") ] on:_useFree ? 0 : 1 enabled:@[ @([d canUseFree]), @(!d.boot) ] in:r rects:_segRects];
}

- (void)drawType:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW];
    Disk *d = [self selectedDisk];
    y = [self cardTitle:NSLocalizedString(@"Disk", @"") at:y];
    [self card:NSMakeRect(x, y, w, 60 + 44)];
    [self diskBar:[self plannedSlices] in:NSMakeRect(x + 12, y + 12, w - 24, 26)];
    [self barKey:@[ @[ @"EFI", [self sliceColour:@"esp"] ], @[ NSLocalizedString(@"Kept", @""), [self sliceColour:@"keep"] ],
                    @[ [NSString stringWithFormat:NSLocalizedString(@"PureDarwin, %@", @""), sizeString([self plannedBytes])], accent() ] ]
               y:y + 44 x:x + 12];
    NSRect r = NSMakeRect(x, y + 60, w, 44);
    [self row:r title:NSLocalizedString(@"Format", @"") sub:NSLocalizedString(@"ext4, journaled, readable from Linux", @"") divider:YES];
    [self segment:@[ @"ext4", @"APFS" ] on:0 enabled:@[ @YES, @NO ] in:r rects:NULL];
    y += 104 + 18;

    y = [self cardTitle:NSLocalizedString(@"What happens", @"") at:y];
    NSArray *steps = _useFree
        ? @[ [NSString stringWithFormat:NSLocalizedString(@"A new partition is made in the %1$@ of free space on %2$@", @""), sizeString(d.freeBytes), d.name],
             NSLocalizedString(@"The system on this drive is copied onto it", @""), NSLocalizedString(@"The loader goes next to the other systems' in the EFI partition", @"") ]
        : @[ [NSString stringWithFormat:NSLocalizedString(@"%@ is erased and gets a new partition table", @""), d.name],
             NSLocalizedString(@"The system on this drive is copied onto it", @""), NSLocalizedString(@"The loader is installed, so the firmware can start it", @"") ];
    [self card:NSMakeRect(x, y, w, [steps count] * 36)];
    for (NSUInteger i = 0; i < [steps count]; i++)
        [self row:NSMakeRect(x, y + i * 36, w, 36) title:steps[i] sub:nil divider:i > 0];
}

- (void)drawInstall:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW];
    y = [self lead:NSLocalizedString(@"You can keep using the desktop while this runs. Don't remove the install drive.", @"") at:y];
    NSDictionary *na = text(@"Inter-Regular", 12.5, P.fg), *pa = text(@"Inter-Regular", 12.5, P.muted);
    [(_now ?: NSLocalizedString(@"Preparing…", @"")) drawAtPoint:NSMakePoint(x, y) withAttributes:na];
    NSString *pct = [NSString stringWithFormat:NSLocalizedString(@"%.0f%%", @""), _progress];
    [pct drawAtPoint:NSMakePoint(x + w - [pct sizeWithAttributes:pa].width, y) withAttributes:pa];
    y += 24;
    [self fillRound:NSMakeRect(x, y, w, 6) radius:3 colour:rgba(127, 127, 127, 0.18)];
    if (_progress > 0)
        [self fillRound:NSMakeRect(x, y, MAX(6, w * _progress / 100.0), 6) radius:3 colour:accent()];
    y += 20;
    NSString *t = _details ? NSLocalizedString(@"▾ Hide Details", @"") : NSLocalizedString(@"▸ Show Details", @"");
    NSDictionary *ta = text(@"Inter-Regular", 12, accent());
    [t drawAtPoint:NSMakePoint(x, y) withAttributes:ta];
    _detailsRect = NSMakeRect(x, y, [t sizeWithAttributes:ta].width, 18);
    y += 26;
    if (!_details)
        return;

    NSRect box = NSMakeRect(x, y, w, WIN_H - BAR_H - 16 - y);
    [self fillRound:box radius:8 colour:rgb(0x1f, 0x20, 0x23)];
    NSDictionary *mono = text(@"Menlo-Regular", 11, rgb(0xd8, 0xd8, 0xd8));
    NSDictionary *hash = text(@"Menlo-Regular", 11, rgb(0x2f, 0xbf, 0x71));
    NSDictionary *dim = text(@"Menlo-Regular", 11, rgb(0x8a, 0x8a, 0x8a));
    NSUInteger rows = (NSUInteger)((NSHeight(box) - 16) / 16);
    NSUInteger first = [_log count] > rows ? [_log count] - rows : 0;
    CGFloat ly = NSMinY(box) + 9;
    for (NSUInteger i = first; i < [_log count]; i++, ly += 16) {
        NSString *l = _log[i];
        BOOL cmd = [l hasPrefix:@"# "];
        if (cmd) {
            [@"#" drawAtPoint:NSMakePoint(NSMinX(box) + 11, ly) withAttributes:hash];
            l = [l substringFromIndex:2];
        }
        if ([l length] > 78)
            l = [[l substringToIndex:77] stringByAppendingString:@"…"];
        [l drawAtPoint:NSMakePoint(NSMinX(box) + (cmd ? 25 : 11), ly) withAttributes:cmd ? mono : dim];
    }
}

- (void)drawDone:(CGFloat)y
{
    CGFloat x = [self contentX], w = [self contentW];
    NSRect tick = NSMakeRect(x + w / 2 - 32, y + 16, 64, 64);
    [(_error ? rgb(0xd9, 0x4f, 0x45) : accent()) setFill];
    [[NSBezierPath bezierPathWithOvalInRect:tick] fill];
    NSBezierPath *mark = [NSBezierPath bezierPath];
    [[NSColor whiteColor] setStroke];
    [mark setLineWidth:3.5];
    [mark setLineCapStyle:NSRoundLineCapStyle];
    if (_error) {
        [mark moveToPoint:NSMakePoint(NSMidX(tick), NSMinY(tick) + 17)];
        [mark lineToPoint:NSMakePoint(NSMidX(tick), NSMinY(tick) + 38)];
        [mark moveToPoint:NSMakePoint(NSMidX(tick), NSMinY(tick) + 46)];
        [mark lineToPoint:NSMakePoint(NSMidX(tick), NSMinY(tick) + 47)];
    } else {
        [mark setLineJoinStyle:NSRoundLineJoinStyle];
        [mark moveToPoint:NSMakePoint(NSMinX(tick) + 19, NSMinY(tick) + 33)];
        [mark lineToPoint:NSMakePoint(NSMinX(tick) + 28, NSMinY(tick) + 42)];
        [mark lineToPoint:NSMakePoint(NSMinX(tick) + 45, NSMinY(tick) + 23)];
    }
    [mark stroke];
    y += 98;

    NSString *head = _error ? NSLocalizedString(@"The install did not finish", @"") : NSLocalizedString(@"PureDarwin is installed", @"");
    NSDictionary *ha = text(@"Inter-Bold", 20, P.fg);
    [head drawAtPoint:NSMakePoint(floor(x + w / 2 - [head sizeWithAttributes:ha].width / 2), y) withAttributes:ha];
    y += 36;
    NSString *body = _error ?: NSLocalizedString(@"Remove the install drive and restart. The firmware starts the new system from its PureDarwin entry.", @"");
    NSDictionary *ba = text(@"Inter-Regular", 13, P.lead);
    CGFloat bw = MIN(w, 380), by = y;
    // centred lines: wrap off screen width first, then draw each line centred
    NSMutableArray *lines = [NSMutableArray array];
    NSMutableString *line = [NSMutableString string];
    for (NSString *word in [body componentsSeparatedByString:@" "]) {
        NSString *next = [line length] ? [NSString stringWithFormat:@"%@ %@", line, word] : word;
        if ([line length] && [next sizeWithAttributes:ba].width > bw) {
            [lines addObject:[[line copy] autorelease]];
            [line setString:word];
        } else {
            [line setString:next];
        }
    }
    [lines addObject:line];
    for (NSString *l in lines) {
        [l drawAtPoint:NSMakePoint(floor(x + w / 2 - [l sizeWithAttributes:ba].width / 2), by) withAttributes:ba];
        by += 20;
    }
    y = by + 18;

    Disk *d = [self selectedDisk];
    NSArray *rows = _error ? @[ @[ NSLocalizedString(@"Log", @""), @"/tmp/pdinstall.log" ] ]
                           : @[ @[ NSLocalizedString(@"Installed on", @""), _result ?: d.name ?: @"" ], @[ NSLocalizedString(@"Format", @""), NSLocalizedString(@"ext4, journaled", @"") ] ];
    [self card:NSMakeRect(x, y, w, [rows count] * 38)];
    for (NSUInteger i = 0; i < [rows count]; i++) {
        NSRect r = NSMakeRect(x, y + i * 38, w, 38);
        [self row:r title:rows[i][0] sub:nil divider:i > 0];
        [self valueText:rows[i][1] in:r];
    }
}

// the licence's agree sheet, dropped from the top of the pane
- (void)drawSheet
{
    [rgba(0, 0, 0, 0.12) setFill];
    NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);
    NSRect s = NSMakeRect(WIN_W / 2 - 210, 0, 420, 128);
    [self fillRound:NSOffsetRect(s, 0, 4) radius:12 colour:rgba(0, 0, 0, 0.18)];
    [self fillRound:s radius:12 colour:P.bg];
    [NSLocalizedString(@"To continue, agree to the terms of the licence.", @"") drawAtPoint:NSMakePoint(NSMinX(s) + 20, 20)
                                                    withAttributes:text(@"Inter-SemiBold", 13, P.fg)];
    [self wrap:NSLocalizedString(@"Choose Agree to carry on with the install, or Disagree to stop and keep using PureDarwin from the install drive.", @"")
             x:NSMinX(s) + 20 y:42 width:380 attrs:text(@"Inter-Regular", 12, P.lead) lineHeight:18];
    NSDictionary *pa = text(@"Inter-Regular", 13, [NSColor whiteColor]), *da = text(@"Inter-Regular", 13, P.fg);
    _agreeRect = NSMakeRect(NSMaxX(s) - 20 - 92, 88, 92, 28);
    _disagreeRect = NSMakeRect(NSMinX(_agreeRect) - 8 - 92, 88, 92, 28);
    [self fillRound:NSInsetRect(_disagreeRect, -1, -1) radius:7 colour:rgba(0, 0, 0, 0.15)];
    [self fillRound:_disagreeRect radius:6 colour:P.button];
    [self fillRound:_agreeRect radius:6 colour:accent()];
    [NSLocalizedString(@"Disagree", @"") drawAtPoint:NSMakePoint(NSMidX(_disagreeRect) - [NSLocalizedString(@"Disagree", @"") sizeWithAttributes:da].width / 2, 93) withAttributes:da];
    [NSLocalizedString(@"Agree", @"") drawAtPoint:NSMakePoint(NSMidX(_agreeRect) - [NSLocalizedString(@"Agree", @"") sizeWithAttributes:pa].width / 2, 93) withAttributes:pa];
}

- (NSString *)goTitle
{
    switch (_step) {
    case StepNetwork: return [self connected] ? NSLocalizedString(@"Continue", @"") : NSLocalizedString(@"Continue Without a Network", @"");
    case StepType: return _useFree ? NSLocalizedString(@"Install", @"") : NSLocalizedString(@"Erase and Install", @"");
    case StepInstall: return NSLocalizedString(@"Continue", @"");
    case StepDone: return _error ? NSLocalizedString(@"Quit", @"") : NSLocalizedString(@"Restart", @"");
    default: return NSLocalizedString(@"Continue", @"");
    }
}

- (BOOL)goEnabled
{
    if (_step == StepInstall)
        return NO;
    if (_step == StepDestination)
        return [self destinationOK];
    return YES;
}

- (BOOL)backEnabled
{
    return _step > StepIntro && _step < StepInstall;
}

- (NSString *)note
{
    Disk *d = [self selectedDisk];
    if (_step == StepNetwork && ![self connected])
        return NSLocalizedString(@"You can connect later in Settings.", @"");
    if (_step == StepDestination && d != nil && d.boot)
        return NSLocalizedString(@"Pick another disk.", @"");
    if (_step == StepDestination && d != nil && ![self destinationOK])
        return NSLocalizedString(@"Not enough free space on this disk.", @"");
    if (_step == StepType && !_useFree)
        return [NSString stringWithFormat:NSLocalizedString(@"Everything on %@ is deleted.", @""), d.name];
    return @"";
}

- (void)drawRect:(NSRect)dirty
{
    [_hits removeAllObjects];
    [P.bg setFill];
    NSRectFill([self bounds]);
    [P.side setFill];
    NSRectFill(NSMakeRect(0, 0, SIDE_W, WIN_H));
    [P.line setFill];
    NSRectFill(NSMakeRect(SIDE_W - 1, 0, 1, WIN_H));

    // the steps down the sidebar
    for (int i = 0; i < StepCount; i++) {
        CGFloat y = 46 + i * 28;
        BOOL done = i < _step || (_step == StepDone && i == _step && !_error);
        BOOL now = i == _step && !done;
        NSColor *c = now ? P.stepNow : done ? P.stepDone : P.stepTodo;
        if (now)
            [self fillRound:NSMakeRect(12, y, SIDE_W - 24, 26) radius:6 colour:rgba(127, 127, 127, 0.14)];
        NSRect dot = NSMakeRect(20, y + 5, 16, 16);
        if (done) {
            [accent() setFill];
            [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
            NSBezierPath *m = [NSBezierPath bezierPath];
            [m moveToPoint:NSMakePoint(NSMinX(dot) + 4.5, NSMinY(dot) + 8.5)];
            [m lineToPoint:NSMakePoint(NSMinX(dot) + 7, NSMinY(dot) + 11)];
            [m lineToPoint:NSMakePoint(NSMinX(dot) + 11.5, NSMinY(dot) + 5.5)];
            [m setLineWidth:1.6];
            [[NSColor whiteColor] setStroke];
            [m stroke];
        } else if (now) {
            [accent() setFill];
            [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
            [P.side setFill];
            [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dot, 4.5, 4.5)] fill];
        } else {
            NSBezierPath *ring = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dot, 0.75, 0.75)];
            [ring setLineWidth:1.5];
            [[c colorWithAlphaComponent:0.7] setStroke];
            [ring stroke];
        }
        [[[NSBundle mainBundle] localizedStringForKey:stepNames[i] value:nil table:nil] drawAtPoint:NSMakePoint(46, y + 5) withAttributes:text(now ? @"Inter-SemiBold" : @"Inter-Regular", 13, c)];
    }
    [_footLogo drawInRect:NSMakeRect(20, WIN_H - 32, 18, 18) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:0.55
           respectFlipped:YES hints:nil];
    [_version drawAtPoint:NSMakePoint(46, WIN_H - 31) withAttributes:text(@"Inter-Regular", 11, P.muted)];

    // the title bar of the pane and the button bar
    [P.line setFill];
    NSRectFill(NSMakeRect(SIDE_W, HEAD_H - 1, WIN_W - SIDE_W, 1));
    NSRectFill(NSMakeRect(SIDE_W, WIN_H - BAR_H, WIN_W - SIDE_W, 1));
    NSString *titles[StepCount] = { NSLocalizedString(@"Welcome to PureDarwin", @""), NSLocalizedString(@"Licence", @""), NSLocalizedString(@"Connect to a Network", @""), NSLocalizedString(@"Select a Destination", @""),
                                    NSLocalizedString(@"Installation Type", @""), NSLocalizedString(@"Installing", @""), NSLocalizedString(@"Summary", @"") };
    [titles[_step] drawAtPoint:NSMakePoint(SIDE_W + PAD, 17) withAttributes:text(@"Inter-Bold", 14, P.fg)];
    NSString *count = [NSString stringWithFormat:NSLocalizedString(@"Step %1$d of %2$d", @""), _step + 1, StepCount];
    NSDictionary *ca = text(@"Inter-Regular", 12, P.muted);
    [count drawAtPoint:NSMakePoint(WIN_W - PAD - [count sizeWithAttributes:ca].width, 18) withAttributes:ca];

    CGFloat y = HEAD_H + 20;
    switch (_step) {
    case StepIntro: [self drawIntro:y]; break;
    case StepLicence: [self drawLicence:y]; break;
    case StepNetwork: [self drawNetwork:y]; break;
    case StepDestination: [self drawDestination:y]; break;
    case StepType: [self drawType:y]; break;
    case StepInstall: [self drawInstall:y]; break;
    default: [self drawDone:y]; break;
    }

    _goRect = [self button:[self goTitle] rightEdge:WIN_W - 18 primary:YES enabled:[self goEnabled]];
    _backRect = NSZeroRect;
    if ([self backEnabled])
        _backRect = [self button:NSLocalizedString(@"Go Back", @"") rightEdge:NSMinX(_goRect) - 8 primary:NO enabled:YES];
    NSString *note = [self note];
    if ([note length])
        [note drawAtPoint:NSMakePoint(SIDE_W + 18, WIN_H - BAR_H + 19) withAttributes:text(@"Inter-Regular", 12, P.muted)];
    if (_sheet)
        [self drawSheet];
}

// one protocol line from pdinstall
- (void)handleLine:(NSString *)l
{
    if ([l hasPrefix:@"step "]) {
        [_now release];
        _now = [[[l substringFromIndex:5] stringByAppendingString:@"…"] retain];
        [_log addObject:[l substringFromIndex:5]];
    } else if ([l hasPrefix:@"progress "]) {
        _progress = [[l substringFromIndex:9] doubleValue];
    } else if ([l hasPrefix:@"run "]) {
        [_log addObject:[@"# " stringByAppendingString:[l substringFromIndex:4]]];
    } else if ([l hasPrefix:@"error "]) {
        [_error release];
        _error = [[l substringFromIndex:6] retain];
        [_log addObject:l];
    } else if ([l hasPrefix:@"done "]) {
        NSString *r = [l substringFromIndex:5];
        NSRange on = [r rangeOfString:@" on "];
        [_result release];
        _result = [(on.location != NSNotFound ? [r substringFromIndex:NSMaxRange(on)] : r) retain];
        _progress = 100;
    }
}

- (void)readInstaller:(int)fd
{
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n > 0) {
        [_pending appendString:[[[NSString alloc] initWithBytes:buf length:(NSUInteger)n encoding:NSUTF8StringEncoding] autorelease]];
        NSRange nl;
        while ((nl = [_pending rangeOfString:@"\n"]).location != NSNotFound) {
            [self handleLine:[_pending substringToIndex:nl.location]];
            [_pending deleteCharactersInRange:NSMakeRange(0, nl.location + 1)];
        }
        [self setNeedsDisplay:YES];
        return;
    }
    // end of output, the installer has exited
    dispatch_source_cancel(_source);
    int status = 0;
    waitpid(_child, &status, 0);
    close(fd);
    if (!_error && (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || !_result))
        _error = [NSLocalizedString(@"pdinstall stopped before it finished.", @"") retain];
    _step = StepDone;
    [self setNeedsDisplay:YES];
}

- (void)startInstall
{
    Disk *d = [self selectedDisk];
    int fds[2];
    posix_spawn_file_actions_t fa;
    const char *argv[] = { pdinstallPath(), _useFree ? "free" : "erase", [d.name UTF8String], "-y", NULL };

    if (pipe(fds) != 0)
        return;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 2);
    posix_spawn_file_actions_addclose(&fa, fds[0]);
    int rc = posix_spawn(&_child, argv[0], &fa, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);
    if (rc != 0) {
        close(fds[0]);
        _error = [[NSString stringWithFormat:NSLocalizedString(@"pdinstall could not be started (%s).", @""), strerror(rc)] retain];
        _step = StepDone;
        return;
    }
    _step = StepInstall;
    int fd = fds[0];
    _source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)fd, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(_source, ^{
        [self readInstaller:fd];
    });
    dispatch_resume(_source);
}

- (void)advance
{
    switch (_step) {
    case StepLicence:
        if (!_agreed) {
            _sheet = YES;
            break;
        }
        [self loadNetworks];
        [[PDWiFi shared] refresh];
        [[PDWiFi shared] scan];
        _step = StepNetwork;
        break;
    case StepNetwork:
        [self loadDisks];
        _step = StepDestination;
        break;
    case StepType:
        [self startInstall];
        break;
    case StepDone:
        if (_error)
            [NSApp terminate:self];
        else
            execl("/sbin/reboot", "reboot", (char *)NULL);
        break;
    default:
        _step++;
        break;
    }
    [self setNeedsDisplay:YES];
}

- (void)agree:(BOOL)yes
{
    _sheet = NO;
    if (yes) {
        _agreed = YES;
        [self advance];
    }
    [self setNeedsDisplay:YES];
}

- (void)back
{
    if (![self backEnabled])
        return;
    _step--;
    if (_step == StepNetwork)
        [self loadNetworks];
    [self setNeedsDisplay:YES];
}

- (void)pickDisk:(NSInteger)i
{
    if (i < 0 || i >= (NSInteger)MIN([_disks count], 6) || [_disks[i] boot])
        return;
    _selected = i;
    [self chooseMode];
    [self setNeedsDisplay:YES];
}

- (void)pickMode:(BOOL)useFree
{
    Disk *d = [self selectedDisk];
    if (d == nil || d.boot || (useFree && ![d canUseFree]))
        return;
    _useFree = useFree;
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (_sheet) {
        if (NSPointInRect(p, _agreeRect))
            [self agree:YES];
        else if (NSPointInRect(p, _disagreeRect))
            [self agree:NO];
        return;
    }
    for (Hit *h in [[_hits copy] autorelease]) {
        if (NSPointInRect(p, h->rect)) {
            h->action();
            [self setNeedsDisplay:YES];
            return;
        }
    }
    if (NSPointInRect(p, _goRect) && [self goEnabled]) {
        [self advance];
        return;
    }
    if (NSPointInRect(p, _backRect)) {
        [self back];
        return;
    }
    if (_step == StepInstall && NSPointInRect(p, _detailsRect)) {
        _details = !_details;
        [self setNeedsDisplay:YES];
        return;
    }
    if (_step != StepDestination)
        return;
    for (NSUInteger i = 0; i < MIN([_disks count], 6); i++) {
        if (NSPointInRect(p, _diskRects[i])) {
            [self pickDisk:(NSInteger)i];
            return;
        }
    }
    for (int m = 0; m < 2; m++) {
        if (NSPointInRect(p, _segRects[m])) {
            [self pickMode:m == 0];
            return;
        }
    }
}

// return goes on, escape goes back, the arrows pick a disk and up or down the way it is used
- (void)keyDown:(NSEvent *)event
{
    NSString *k = [event charactersIgnoringModifiers];
    unichar c = [k length] ? [k characterAtIndex:0] : 0;

    if (_sheet) {
        if (c == '\r' || c == '\n')
            [self agree:YES];
        else if (c == 27)
            [self agree:NO];
        return;
    }
    if ((c == '\r' || c == '\n') && [self goEnabled])
        [self advance];
    else if (c == 27)
        [self back];
    else if (_step == StepDestination && (c == 0xF702 || c == 0xF703))
        [self pickDisk:_selected + (c == 0xF703 ? 1 : -1)];
    else if (_step == StepDestination && (c == 0xF700 || c == 0xF701))
        [self pickMode:c == 0xF700];
    else if (_step == StepInstall && c == 'd') {
        _details = !_details;
        [self setNeedsDisplay:YES];
    } else
        [super keyDown:event];
}

@end

@interface InstallerController : NSObject <NSApplicationDelegate, NSWindowDelegate> {
    NSWindow *_window;
}
@end

@implementation InstallerController

- (void)buildMenu
{
    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSMenu *app = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Install PureDarwin", @"")];
    NSMenuItem *top = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Install PureDarwin", @"") action:NULL keyEquivalent:@""];
    NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Quit Install PureDarwin", @"") action:@selector(terminate:) keyEquivalent:@"q"];

    [app addItem:quit];
    [top setSubmenu:app];
    [mainMenu addItem:top];
    [NSApp setMainMenu:mainMenu];
    [quit release];
    [top release];
    [app release];
    [mainMenu release];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect screen = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(NSMidX(screen) - WIN_W / 2, NSMidY(screen) - WIN_H / 2, WIN_W, WIN_H);

    loadPalette();
    [self buildMenu];
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
                                                    NSFullSizeContentViewWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:NSLocalizedString(@"Install PureDarwin", @"")];
    [_window setTitlebarAppearsTransparent:YES];
    [_window setTitleVisibility:NSWindowTitleHidden];
    [_window setBackgroundColor:P.bg];
    [_window setDelegate:self];
    // the full-size content view covers the title bar too, so the frame is WIN_H and the lights sit on the sidebar
    NSRect f = [_window frame];
    f.origin.y += NSHeight(f) - WIN_H;
    f.size.height = WIN_H;
    [_window setFrame:f display:NO];
    InstallerView *view = [[InstallerView alloc] initWithFrame:NSMakeRect(0, 0, WIN_W, WIN_H)];
    [_window setContentView:view];
    [view setFrame:NSMakeRect(0, 0, WIN_W, WIN_H)];
    [_window makeFirstResponder:view];
    [view release];
    [_window makeKeyAndOrderFront:nil];
}

- (void)windowWillClose:(NSNotification *)note
{
    [NSApp terminate:self];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    InstallerController *controller = [[InstallerController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
