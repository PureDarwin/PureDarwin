// main.m - Disks: the drives, their partitions and volumes, how full each is, mount and unmount
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// Erase, Partition, Check and New Disk Image stay disabled until the command-line tool exists

#import <AppKit/AppKit.h>
#import "PDUtility.h"
#include <IOKit/IOKitLib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

#define SIDE_W 248
#define DISK_ROW 40
#define VOL_ROW 22

@interface Volume : NSObject {
@public
    NSString *bsd;
    NSString *name;
    NSString *fs;
    NSString *type;
    NSString *typeGUID;
    NSString *uuid;
    NSString *mount;
    double size;
    double used;
    unsigned color;
    BOOL boot;
}
@end

@implementation Volume

- (void)dealloc
{
    [bsd release];
    [name release];
    [fs release];
    [type release];
    [typeGUID release];
    [uuid release];
    [mount release];
    [super dealloc];
}

@end

@interface Disk : NSObject {
@public
    NSString *bsd;
    NSString *name;
    NSString *bus;
    NSString *scheme;
    double size;
    unsigned blockSize;
    BOOL internal;
    BOOL image;
    BOOL removable;
    NSMutableArray *vols;
}
@end

@implementation Disk

- (void)dealloc
{
    [bsd release];
    [name release];
    [bus release];
    [scheme release];
    [vols release];
    [super dealloc];
}

@end

// the calm palette the demo settled on, one colour per kind of volume
static unsigned colorFor(NSString *fs, BOOL boot, NSString *name)
{
    if ([fs hasPrefix:@"FAT"] || [fs isEqualToString:@"EFI"])
        return 0x8e949c;
    if ([fs isEqualToString:@"APFS"])
        return 0x5f9e8f;
    if (boot)
        return 0x4a7fc1;
    if ([[name lowercaseString] rangeOfString:@"recovery"].location != NSNotFound)
        return 0xb98a74;
    if ([[name lowercaseString] rangeOfString:@"install"].location != NSNotFound)
        return 0xc46a5a;
    return 0x8a72b8;
}

// partition type GUIDs to what they hold
static NSArray *typeFor(NSString *guid)
{
    NSDictionary *map = @{
        @"C12A7328-F81F-11D2-BA4B-00A0C93EC93B": @[ @"FAT32", @"EFI System" ],
        @"0FC63DAF-8483-4772-8E79-3D69D8477DE4": @[ @"ext4", @"Linux filesystem" ],
        @"7C3457EF-0000-11AA-AA11-00306543ECAC": @[ @"APFS", @"Apple APFS" ],
        @"EBD0A0A2-B9E5-4433-87C0-68B6B72699C7": @[ @"FAT32", @"Basic data" ],
        @"48465300-0000-11AA-AA11-00306543ECAC": @[ @"HFS+", @"Apple HFS+" ],
        @"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F": @[ @"swap", @"Linux swap" ],
    };

    return map[[guid uppercaseString]] ?: @[ @"Unknown", guid ?: @"Unknown" ];
}

static id prop(io_registry_entry_t e, NSString *key)
{
    return [(id)IORegistryEntryCreateCFProperty(e, (CFStringRef)key, kCFAllocatorDefault, 0) autorelease];
}

static id searchUp(io_registry_entry_t e, NSString *key)
{
    return [(id)IORegistryEntrySearchCFProperty(e, kIOServicePlane, (CFStringRef)key, kCFAllocatorDefault,
                                                kIORegistryIterateRecursively | kIORegistryIterateParents) autorelease];
}

// how the drive is attached, from the storage protocol when published, otherwise from the drivers above it
static NSString *busOf(io_registry_entry_t e, BOOL *image)
{
    NSDictionary *proto = searchUp(e, @"Protocol Characteristics");
    NSString *phys = [proto isKindOfClass:[NSDictionary class]] ? proto[@"Physical Interconnect"] : nil;
    io_registry_entry_t cur = e, parent;
    NSMutableString *classes = [NSMutableString string];

    IOObjectRetain(cur);
    while (IORegistryEntryGetParentEntry(cur, kIOServicePlane, &parent) == KERN_SUCCESS) {
        io_name_t cls;
        IOObjectGetClass(parent, cls);
        [classes appendFormat:@"%s ", cls];
        IOObjectRelease(cur);
        cur = parent;
    }
    IOObjectRelease(cur);
    *image = [phys isEqualToString:@"Virtual Interface"] || [classes rangeOfString:@"DiskImage"].location != NSNotFound;
    if (*image)
        return NSLocalizedString(@"Disk image", @"");
    if ([classes rangeOfString:@"XHCI"].location != NSNotFound || [classes rangeOfString:@"USB"].location != NSNotFound || [phys isEqualToString:@"USB"])
        return @"USB";
    if ([classes rangeOfString:@"NVMe"].location != NSNotFound || [phys isEqualToString:@"PCI-Express"])
        return @"NVMe";
    if ([classes rangeOfString:@"AHCI"].location != NSNotFound || [phys isEqualToString:@"SATA"])
        return @"SATA";
    if ([classes rangeOfString:@"VirtIO"].location != NSNotFound || [classes rangeOfString:@"Virtio"].location != NSNotFound)
        return @"VirtIO";
    if ([classes rangeOfString:@"MMC"].location != NSNotFound || [classes rangeOfString:@"SDHC"].location != NSNotFound)
        return @"SD card";
    return phys ?: NSLocalizedString(@"Internal", @"");
}

static int run(NSArray *args)
{
    char **argv = calloc([args count] + 1, sizeof(char *));
    pid_t pid;
    int status = -1;

    for (NSUInteger i = 0; i < [args count]; i++)
        argv[i] = (char *)[args[i] fileSystemRepresentation];
    if (posix_spawn(&pid, argv[0], NULL, NULL, argv, environ) == 0)
        waitpid(pid, &status, 0);
    free(argv);
    return status;
}

@interface DisksView : NSView {
    NSMutableArray *_disks;
    id _selected;
    NSArray *_buttons;
    PDToolButton *_mount;
    NSMutableArray *_rows;
    NSMutableArray *_partRows;
    CGFloat _scroll;
}
- (void)reload;
- (IBAction)toggleMount:(id)sender;
- (IBAction)mountVolume:(id)sender;
- (IBAction)unmountVolume:(id)sender;
- (IBAction)showInfo:(id)sender;
@end

@implementation DisksView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _disks = [[NSMutableArray alloc] init];
    _rows = [[NSMutableArray alloc] init];
    _partRows = [[NSMutableArray alloc] init];
    _mount = [PDToolButton buttonWithGlyph:@"eject" label:NSLocalizedString(@"Unmount", @"") target:self action:@selector(toggleMount:)];
    _buttons = [@[ [PDToolButton buttonWithGlyph:@"check" label:NSLocalizedString(@"Check", @"") target:nil action:NULL],
                   [PDToolButton buttonWithGlyph:@"partition" label:NSLocalizedString(@"Partition", @"") target:nil action:NULL],
                   [PDToolButton buttonWithGlyph:@"erase" label:NSLocalizedString(@"Erase", @"") target:nil action:NULL],
                   _mount, [PDToolButton buttonWithGlyph:@"info" label:nil target:self action:@selector(showInfo:)] ] retain];
    // the destructive actions are placeholders until the command-line tool behind them exists
    for (int i = 0; i < 3; i++)
        ((PDToolButton *)_buttons[i])->enabled = NO;
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self reload];
    return self;
}

- (void)dealloc
{
    [_disks release];
    [_rows release];
    [_partRows release];
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

- (void)reload
{
    NSMutableDictionary *mounts = [NSMutableDictionary dictionary], *byName = [NSMutableDictionary dictionary];
    struct statfs *fsl = NULL;
    int nfs = getmntinfo(&fsl, MNT_NOWAIT);
    io_iterator_t iter;
    io_registry_entry_t e;
    NSString *selected = [_selected isKindOfClass:[Disk class]] ? ((Disk *)_selected)->bsd : [_selected isKindOfClass:[Volume class]] ? ((Volume *)_selected)->bsd : nil;

    // by device number too, the root is mounted from "root_device" and not a /dev path
    for (int i = 0; i < nfs; i++) {
        NSString *from = [NSString stringWithUTF8String:fsl[i].f_mntfromname];
        struct stat st;

        if ([from hasPrefix:@"/dev/"])
            mounts[[from substringFromIndex:5]] = [NSValue valueWithPointer:&fsl[i]];
        if (stat(fsl[i].f_mntonname, &st) == 0 && strcmp(fsl[i].f_fstypename, "devfs") != 0)
            mounts[[NSString stringWithFormat:@"dev:%d", (int)st.st_dev]] = [NSValue valueWithPointer:&fsl[i]];
    }
    [_disks removeAllObjects];
    if (IOServiceGetMatchingServices(MACH_PORT_NULL, IOServiceMatching("IOMedia"), &iter) != KERN_SUCCESS)
        return;
    // whole disks first, then each partition goes under its disk
    NSMutableArray *parts = [NSMutableArray array];
    while ((e = IOIteratorNext(iter)) != 0) {
        NSString *bsd = prop(e, @"BSD Name");

        if (bsd == nil) {
            IOObjectRelease(e);
            continue;
        }
        if ([prop(e, @"Whole") boolValue]) {
            Disk *d = [[[Disk alloc] init] autorelease];
            NSDictionary *dev = searchUp(e, @"Device Characteristics");
            NSString *product = [dev isKindOfClass:[NSDictionary class]] ? dev[@"Product Name"] : nil;
            NSString *vendor = [dev isKindOfClass:[NSDictionary class]] ? dev[@"Vendor Name"] : nil;
            io_name_t n;

            d->bsd = [bsd copy];
            d->size = [prop(e, @"Size") doubleValue];
            d->blockSize = [prop(e, @"Preferred Block Size") unsignedIntValue] ?: 512;
            d->removable = [prop(e, @"Removable") boolValue] || [prop(e, @"Ejectable") boolValue];
            d->bus = [busOf(e, &d->image) copy];
            d->internal = !d->removable && !d->image && ![d->bus isEqualToString:@"USB"];
            d->scheme = [([prop(e, @"Content") isEqualToString:@"GUID_partition_scheme"] ? NSLocalizedString(@"GUID Partition Map", @"")
                         : [prop(e, @"Content") isEqualToString:@"FDisk_partition_scheme"] ? NSLocalizedString(@"Master Boot Record", @"")
                         : NSLocalizedString(@"None", @"")) copy];
            IORegistryEntryGetName(e, n);
            d->name = [(product != nil ? [[NSString stringWithFormat:@"%@ %@", vendor ?: @"", product] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
                        : [NSString stringWithUTF8String:n]) copy];
            if (d->image && [d->name length] == 0)
                d->name = [NSLocalizedString(@"Disk image", @"") copy];
            d->vols = [[NSMutableArray alloc] init];
            [_disks addObject:d];
            byName[bsd] = d;
        } else {
            Volume *v = [[[Volume alloc] init] autorelease];
            NSString *guid = prop(e, @"Content");
            NSArray *t = typeFor(guid);
            NSValue *m = mounts[bsd] ?: mounts[[NSString stringWithFormat:@"dev:%d", (int)makedev([prop(e, @"BSD Major") intValue], [prop(e, @"BSD Minor") intValue])]];
            io_name_t n;

            v->bsd = [bsd copy];
            v->size = [prop(e, @"Size") doubleValue];
            v->typeGUID = [guid copy];
            v->uuid = [prop(e, @"UUID") copy];
            v->fs = [t[0] copy];
            v->type = [t[1] copy];
            IORegistryEntryGetName(e, n);
            if (m != nil) {
                struct statfs *s = [m pointerValue];
                v->mount = [[NSString stringWithUTF8String:s->f_mntonname] copy];
                v->size = (double)s->f_blocks * s->f_bsize;
                v->used = (double)(s->f_blocks - s->f_bfree) * s->f_bsize;
                v->boot = [v->mount isEqualToString:@"/"];
                [v->fs release];
                v->fs = [([@(s->f_fstypename) isEqualToString:@"msdos"] ? @"FAT32" : [@(s->f_fstypename) isEqualToString:@"apfs"] ? @"APFS" : @(s->f_fstypename)) copy];
            }
            v->name = [(v->boot ? @"PureDarwin" : v->mount != nil && [v->mount hasPrefix:@"/Volumes/"] ? [v->mount lastPathComponent]
                        : [@(n) length] > 0 && strncmp(n, "IOMedia", 7) != 0 && strncmp(n, "Untitled", 8) != 0 ? @(n)
                        : [t[1] isEqualToString:@"EFI System"] ? @"EFI" : bsd) copy];
            v->color = colorFor(v->fs, v->boot, v->name);
            [parts addObject:v];
        }
        IOObjectRelease(e);
    }
    IOObjectRelease(iter);
    for (Volume *v in parts) {
        NSRange s = [v->bsd rangeOfString:@"s" options:NSBackwardsSearch];
        Disk *d = s.location != NSNotFound && s.location > 4 ? byName[[v->bsd substringToIndex:s.location]] : nil;

        [d->vols addObject:v];
    }
    [_disks sortUsingComparator:^NSComparisonResult(Disk *a, Disk *b) { return [a->bsd compare:b->bsd options:NSNumericSearch]; }];
    for (Disk *d in _disks)
        [d->vols sortUsingComparator:^NSComparisonResult(Volume *a, Volume *b) { return [a->bsd compare:b->bsd options:NSNumericSearch]; }];

    _selected = nil;
    for (Disk *d in _disks) {
        if ([d->bsd isEqualToString:selected])
            _selected = d;
        for (Volume *v in d->vols) {
            if ([v->bsd isEqualToString:selected] || (selected == nil && v->boot))
                _selected = v;
        }
    }
    if (_selected == nil && [_disks count] > 0)
        _selected = _disks[0];
    [self setNeedsDisplay:YES];
}

- (Disk *)diskOf:(Volume *)v
{
    for (Disk *d in _disks) {
        if ([d->vols containsObject:v])
            return d;
    }
    return nil;
}

- (NSImage *)glyphForDisk:(Disk *)d
{
    return PDResourceImage(d->image ? @"disc" : @"disks");
}

- (void)drawVolumeGlyph:(Volume *)v in:(NSRect)r
{
    CGFloat dot = floor(NSWidth(r) * .38);

    [PDResourceImage(NSWidth(r) > 20 ? @"folder" : @"folder-small") drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    PDFillRounded(NSMakeRect(NSMaxX(r) - dot + 1, NSMaxY(r) - dot, dot, dot), dot / 2, PDColor(0xffffff, 0x2c2c2e));
    PDFillRounded(NSMakeRect(NSMaxX(r) - dot + 2, NSMaxY(r) - dot + 1, dot - 2, dot - 2), dot / 2, PDFixed(v->color));
}

- (void)drawSidebarIn:(NSRect)side
{
    NSDictionary *bold = PDText(PDFont(@"Inter-SemiBold", 13), PDColor(0x1b1b1b, 0xe6e6e6));
    NSDictionary *small = PDText(PDFont(@"Inter-Regular", 10.5), PDColor(0x8a8580, 0x8c8780));
    NSDictionary *vol = PDText(PDFont(@"Inter-Regular", 12.5), PDColor(0x1b1b1b, 0xe6e6e6));
    NSDictionary *volOff = PDText(PDFont(@"Inter-Regular", 12.5), PDColor(0xa09a95, 0x77736f));
    CGFloat y = NSMinY(side) + 6;

    PDDrawSidebar(side);
    [_rows removeAllObjects];
    for (int g = 0; g < 3; g++) {
        NSString *title = g == 0 ? NSLocalizedString(@"Internal", @"") : g == 1 ? NSLocalizedString(@"External", @"") : NSLocalizedString(@"Disk Images", @"");
        BOOL any = NO;

        for (Disk *d in _disks) {
            int group = d->image ? 2 : d->internal ? 0 : 1;
            NSRect r;

            if (group != g)
                continue;
            if (!any) {
                y += 8;
                PDDrawSectionHeading(title, NSMakePoint(18, y));
                y += 18;
                any = YES;
            }
            r = NSMakeRect(10, y, SIDE_W - 20, DISK_ROW - 4);
            if (_selected == d)
                PDFillRounded(r, 6, PDAlpha(PDColor(0x000000, 0xffffff), .1));
            [[self glyphForDisk:d] drawInRect:NSMakeRect(18, y + 8, 20, 20) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
            [PDFitted(d->name, bold, SIDE_W - 80) drawAtPoint:NSMakePoint(46, y + 2) withAttributes:bold];
            [PDFitted([NSString stringWithFormat:@"%@, %@", PDBytes(d->size), d->bus], small, SIDE_W - 80) drawAtPoint:NSMakePoint(46, y + 19) withAttributes:small];
            if (!d->internal)
                PDDrawGlyph(@"eject", NSMakeRect(SIDE_W - 30, y + 12, 12, 12), PDColor(0x8a8a8a, 0x8a8a8a));
            [_rows addObject:@[ [NSValue valueWithRect:r], d ]];
            y += DISK_ROW;
            for (Volume *v in d->vols) {
                NSString *sz = PDBytes(v->size);
                NSSize ss = [sz sizeWithAttributes:small];

                r = NSMakeRect(10, y, SIDE_W - 20, VOL_ROW);
                if (_selected == v)
                    PDFillRounded(r, 6, PDAlpha(PDColor(0x000000, 0xffffff), .1));
                [self drawVolumeGlyph:v in:NSMakeRect(40, y + 4, 14, 14)];
                [PDFitted(v->name, vol, SIDE_W - 90 - ss.width - 16) drawAtPoint:NSMakePoint(62, y + 2) withAttributes:v->mount != nil ? vol : volOff];
                [sz drawAtPoint:NSMakePoint(SIDE_W - 18 - ss.width, y + 4) withAttributes:small];
                [_rows addObject:@[ [NSValue valueWithRect:r], v ]];
                y += VOL_ROW;
            }
            y += 2;
        }
    }
}

- (void)drawHeader:(NSString *)title sub:(NSString *)sub big:(NSString *)big small:(NSString *)small glyph:(void (^)(NSRect))glyph at:(NSRect)m
{
    NSDictionary *h2 = PDText(PDFont(@"Inter-Bold", 18), PDColor(0x1b1b1b, 0xeeeeee));
    NSDictionary *subA = PDText(PDFont(@"Inter-Regular", 12), PDColor(0x6e6e6e, 0x9a9a9a));
    NSDictionary *bigA = PDText(PDFont(@"Inter-Light", 22), PDColor(0x1b1b1b, 0xeeeeee));
    NSDictionary *smallA = PDText(PDFont(@"Inter-Regular", 11), PDColor(0x8a8580, 0x8c8780));
    NSSize bs = [big sizeWithAttributes:bigA], ss = [small sizeWithAttributes:smallA];
    CGFloat x = NSMinX(m) + 26, y = NSMinY(m) + 22;

    glyph(NSMakeRect(x, y, 52, 52));
    [PDFitted(title, h2, NSWidth(m) - 260) drawAtPoint:NSMakePoint(x + 66, y + 4) withAttributes:h2];
    [PDFitted(sub, subA, NSWidth(m) - 260) drawAtPoint:NSMakePoint(x + 66, y + 30) withAttributes:subA];
    [big drawAtPoint:NSMakePoint(NSMaxX(m) - 26 - bs.width, y + 2) withAttributes:bigA];
    [small drawAtPoint:NSMakePoint(NSMaxX(m) - 26 - ss.width, y + 32) withAttributes:smallA];
}

- (CGFloat)drawDetails:(NSArray *)rows at:(NSRect)m y:(CGFloat)y
{
    NSDictionary *k = PDText(PDFont(@"Inter-Regular", 11.5), PDColor(0x8a8580, 0x8c8780));
    NSDictionary *v = PDText(PDFont(@"Inter-Regular", 11.5), PDColor(0x4a4a4a, 0xc8c8c8));
    NSDictionary *mono = PDText(PDMono(11), PDColor(0x6b6158, 0xb8aca2));
    NSMutableDictionary *head = [[PDText(PDFont(@"Inter-SemiBold", 11), PDColor(0xa39a92, 0x8c8178)) mutableCopy] autorelease];

    [head setObject:@(1.2) forKey:NSKernAttributeName];
    [[NSLocalizedString(@"Details", @"") uppercaseString] drawAtPoint:NSMakePoint(NSMinX(m) + 26, y) withAttributes:head];
    y += 20;
    for (NSArray *r in rows) {
        BOOL code = [r count] > 2;
        [r[0] drawAtPoint:NSMakePoint(NSMinX(m) + 26, y) withAttributes:k];
        [PDFitted(r[1], code ? mono : v, NSWidth(m) - 26 - 170) drawAtPoint:NSMakePoint(NSMinX(m) + 170, y) withAttributes:code ? mono : v];
        y += 19;
    }
    return y;
}

- (void)drawVolume:(Volume *)v in:(NSRect)m
{
    Disk *d = [self diskOf:v];
    double free_ = v->size - v->used;
    int pct = v->size > 0 ? (int)round(100 * v->used / v->size) : 0;
    NSRect bar = NSMakeRect(NSMinX(m) + 26, NSMinY(m) + 96, NSWidth(m) - 52, 14);
    NSDictionary *key = PDText(PDFont(@"Inter-Regular", 12), PDColor(0x6e6e6e, 0x9a9a9a));
    NSDictionary *val = PDText(PDFont(@"Inter-SemiBold", 12), PDColor(0x1b1b1b, 0xeeeeee));
    CGFloat x = NSMinX(bar), y;

    [self drawHeader:v->name sub:[NSString stringWithFormat:NSLocalizedString(@"%@ volume on %@%@", @""), v->fs, d->name, v->boot ? NSLocalizedString(@", startup disk", @"") : @""]
                 big:v->mount != nil ? PDBytes(free_) : @"—"
               small:v->mount != nil ? [NSString stringWithFormat:NSLocalizedString(@"available of %@", @""), PDBytes(v->size)] : NSLocalizedString(@"not mounted", @"")
               glyph:^(NSRect r) { [self drawVolumeGlyph:v in:NSInsetRect(r, 2, 2)]; } at:m];

    PDFillRounded(bar, 7, PDColor(0xe4e1de, 0x3a3a3c));
    if (v->mount != nil)
        PDFillRounded(NSMakeRect(NSMinX(bar), NSMinY(bar), MAX(14, NSWidth(bar) * pct / 100), 14), 7, PDFixed(v->color));
    y = NSMaxY(bar) + 12;
    PDFillRounded(NSMakeRect(x, y + 4, 9, 9), 2, PDFixed(v->color));
    [NSLocalizedString(@"Used", @"") drawAtPoint:NSMakePoint(x + 15, y) withAttributes:key];
    x += 15 + [NSLocalizedString(@"Used", @"") sizeWithAttributes:key].width + 5;
    [(v->mount != nil ? PDBytes(v->used) : @"—") drawAtPoint:NSMakePoint(x, y) withAttributes:val];
    x += [(v->mount != nil ? PDBytes(v->used) : @"—") sizeWithAttributes:val].width + 22;
    PDFillRounded(NSMakeRect(x, y + 4, 9, 9), 2, PDColor(0xe4e1de, 0x3a3a3c));
    [NSLocalizedString(@"Free", @"") drawAtPoint:NSMakePoint(x + 15, y) withAttributes:key];
    x += 15 + [NSLocalizedString(@"Free", @"") sizeWithAttributes:key].width + 5;
    [(v->mount != nil ? PDBytes(free_) : @"—") drawAtPoint:NSMakePoint(x, y) withAttributes:val];
    NSString *full = v->mount != nil ? [NSString stringWithFormat:NSLocalizedString(@"%d%% full", @""), pct] : NSLocalizedString(@"Mount it to see usage", @"");
    [full drawAtPoint:NSMakePoint(NSMaxX(bar) - [full sizeWithAttributes:key].width, y) withAttributes:PDText(PDFont(@"Inter-Regular", 12), PDColor(0x8a8580, 0x8c8780))];

    // the three facts that matter most, as tiles
    y += 40;
    NSArray *facts = @[ @[ NSLocalizedString(@"Capacity", @""), PDBytes(v->size), @"" ],
                        @[ NSLocalizedString(@"Format", @""), v->fs, [v->fs isEqualToString:@"ext4"] ? NSLocalizedString(@"journaled, 64bit, extents", @"") : @"" ],
                        @[ NSLocalizedString(@"Mount Point", @""), v->mount ?: NSLocalizedString(@"Not mounted", @""),
                           v->boot ? NSLocalizedString(@"startup volume", @"") : v->mount != nil ? NSLocalizedString(@"read and write", @"") : @"" ] ];
    CGFloat tw = (NSWidth(m) - 52 - 20) / 4;
    x = NSMinX(m) + 26;
    for (NSUInteger i = 0; i < 3; i++) {
        CGFloat w = i == 2 ? tw * 2 : tw;
        NSRect t = NSMakeRect(x, y, w, 72);
        NSDictionary *b = PDText(PDFont(@"Inter-SemiBold", i == 2 ? 14 : 15), PDColor(0x1b1b1b, 0xeeeeee));

        PDFillRounded(t, 9, PDColor(0xf7f5f3, 0x2a2a2c));
        [facts[i][0] drawAtPoint:NSMakePoint(x + 12, y + 10) withAttributes:PDText(PDFont(@"Inter-Regular", 11), PDColor(0x8a8580, 0x8c8780))];
        [PDFitted(facts[i][1], b, w - 24) drawAtPoint:NSMakePoint(x + 12, y + 26) withAttributes:b];
        [facts[i][2] drawAtPoint:NSMakePoint(x + 12, y + 48) withAttributes:PDText(PDFont(@"Inter-Regular", 11), PDColor(0x8a8580, 0x8c8780))];
        x += w + 10;
    }
    y += 98;
    [self drawDetails:@[ @[ NSLocalizedString(@"Device", @""), [@"/dev/" stringByAppendingString:v->bsd], @YES ],
                         @[ NSLocalizedString(@"Partition Type", @""), v->type ],
                         @[ NSLocalizedString(@"Type GUID", @""), v->typeGUID ?: @"—", @YES ],
                         @[ NSLocalizedString(@"Volume UUID", @""), v->uuid ?: @"—", @YES ],
                         @[ NSLocalizedString(@"Connection", @""), d->bus ],
                         @[ NSLocalizedString(@"Bootable", @""), v->boot || [v->fs isEqualToString:@"FAT32"] ? NSLocalizedString(@"Yes", @"") : NSLocalizedString(@"No", @"") ] ]
                   at:m y:y];
}

- (void)drawDisk:(Disk *)d in:(NSRect)m
{
    NSRect bar = NSMakeRect(NSMinX(m) + 26, NSMinY(m) + 96, NSWidth(m) - 52, 30);
    CGFloat x = NSMinX(bar), y;

    [self drawHeader:d->name sub:d->bus big:PDBytes(d->size)
               small:[NSString stringWithFormat:NSLocalizedString(@"%lu partitions, %@", @""), (unsigned long)[d->vols count], d->scheme]
               glyph:^(NSRect r) { [[self glyphForDisk:d] drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil]; } at:m];

    // the partition map, wide partitions carry their name
    PDFillRounded(bar, 8, PDColor(0xe4e1de, 0x3a3a3c));
    [NSGraphicsContext saveGraphicsState];
    [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:8 yRadius:8] addClip];
    for (Volume *v in d->vols) {
        CGFloat w = MAX(3, NSWidth(bar) * v->size / MAX(d->size, 1));
        NSDictionary *a = PDText(PDFont(@"Inter-SemiBold", 11), [NSColor whiteColor]);

        [PDFixed(v->color) setFill];
        NSRectFill(NSMakeRect(x, NSMinY(bar), w, NSHeight(bar)));
        [PDColor(0xffffff, 0x1f1f21) setFill];
        NSRectFill(NSMakeRect(x + w - 1, NSMinY(bar), 1, NSHeight(bar)));
        if (w > 120) {
            NSString *n = PDFitted(v->name, a, w - 16);
            [n drawAtPoint:NSMakePoint(x + w / 2 - [n sizeWithAttributes:a].width / 2, NSMidY(bar) - 8) withAttributes:a];
        }
        x += w;
    }
    [NSGraphicsContext restoreGraphicsState];

    // one row per partition, a click opens it
    y = NSMaxY(bar) + 18;
    [_partRows removeAllObjects];
    NSRect list = NSMakeRect(NSMinX(m) + 26, y, NSWidth(m) - 52, 32 * [d->vols count]);
    [PDColor(0xece8e4, 0x2e2e30) setStroke];
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(list, .5, .5) xRadius:9 yRadius:9] stroke];
    for (NSUInteger i = 0; i < [d->vols count]; i++) {
        Volume *v = d->vols[i];
        CGFloat ry = y + i * 32, cx = NSMinX(list) + 12, cw = NSWidth(list) - 24;
        NSDictionary *nameA = PDText(PDFont(@"Inter-SemiBold", 12.5), v->mount != nil ? PDColor(0x1b1b1b, 0xeeeeee) : PDColor(0x8a8a8a, 0x8a8a8a));
        NSDictionary *cell = PDText(PDFont(@"Inter-Regular", 12.5), PDColor(0x6e6e6e, 0xa0a0a0));

        if (i > 0) {
            [PDColor(0xece8e4, 0x2e2e30) setFill];
            NSRectFill(NSMakeRect(NSMinX(list) + 1, ry, NSWidth(list) - 2, 1));
        }
        PDFillRounded(NSMakeRect(cx, ry + 11, 10, 10), 3, PDFixed(v->color));
        [PDFitted(v->name, nameA, cw * .3) drawAtPoint:NSMakePoint(cx + 24, ry + 8) withAttributes:nameA];
        [v->fs drawAtPoint:NSMakePoint(cx + 24 + cw * .32, ry + 8) withAttributes:cell];
        [PDBytes(v->size) drawAtPoint:NSMakePoint(cx + 24 + cw * .46, ry + 8) withAttributes:cell];
        [PDFitted(v->mount ?: NSLocalizedString(@"not mounted", @""), cell, cw * .4) drawAtPoint:NSMakePoint(cx + 24 + cw * .6, ry + 8) withAttributes:cell];
        [_partRows addObject:@[ [NSValue valueWithRect:NSMakeRect(NSMinX(list), ry, NSWidth(list), 32)], v ]];
    }
    y = NSMaxY(list) + 26;
    [self drawDetails:@[ @[ NSLocalizedString(@"Device", @""), [@"/dev/" stringByAppendingString:d->bsd], @YES ],
                         @[ NSLocalizedString(@"Partition Scheme", @""), d->scheme ],
                         @[ NSLocalizedString(@"Connection", @""), d->bus ],
                         @[ NSLocalizedString(@"Location", @""), d->image ? NSLocalizedString(@"Disk image", @"") : d->internal ? NSLocalizedString(@"Internal", @"") : NSLocalizedString(@"External", @"") ],
                         @[ NSLocalizedString(@"Removable", @""), d->removable ? NSLocalizedString(@"Yes", @"") : NSLocalizedString(@"No", @"") ],
                         @[ NSLocalizedString(@"Block Size", @""), [NSString stringWithFormat:NSLocalizedString(@"%u bytes", @""), d->blockSize] ] ]
                   at:m y:y];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSRect m = NSMakeRect(SIDE_W + 1, PD_TOOLBAR_H + 1, NSWidth(b) - SIDE_W - 1, NSHeight(b) - PD_TOOLBAR_H - 1);
    Volume *v = [_selected isKindOfClass:[Volume class]] ? _selected : nil;
    Disk *d = [_selected isKindOfClass:[Disk class]] ? _selected : [self diskOf:v];

    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:8 yRadius:8] addClip];
    PDDrawToolbar(self, NSMakeRect(0, 0, NSWidth(b), PD_TOOLBAR_H));
    PDDrawTitle(NSLocalizedString(@"Disks", @""), v != nil ? [NSString stringWithFormat:@"%@, %@", v->name, v->bsd] : d != nil ? [NSString stringWithFormat:@"%@, %@", d->name, d->bsd] : nil,
                NSMakePoint(PD_LIGHTS_W, PD_TOOLBAR_H / 2), NSWidth(b) - PD_LIGHTS_W - 470, NO);
    [_mount->glyph release];
    [_mount->label release];
    _mount->glyph = [(v != nil && v->mount == nil ? @"mount" : @"eject") copy];
    _mount->label = [(v != nil ? (v->mount == nil ? NSLocalizedString(@"Mount", @"") : NSLocalizedString(@"Unmount", @"")) : NSLocalizedString(@"Eject", @"")) copy];
    _mount->enabled = v != nil ? !v->boot : (d != nil && !d->internal);
    PDLayoutButtons(_buttons, NSWidth(b) - 10, PD_TOOLBAR_H, NO);

    [self drawSidebarIn:NSMakeRect(0, PD_TOOLBAR_H + 1, SIDE_W, NSHeight(b) - PD_TOOLBAR_H - 1)];
    [PDColor(0xffffff, 0x1f1f21) setFill];
    NSRectFill(m);
    if (v != nil)
        [self drawVolume:v in:m];
    else if (d != nil)
        [self drawDisk:d in:m];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (PDClickButtons(_buttons, p))
        return;
    if (p.y < PD_TOOLBAR_H) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    for (NSArray *r in p.x < SIDE_W ? _rows : _partRows) {
        if (NSPointInRect(p, [r[0] rectValue])) {
            _selected = r[1];
            [self setNeedsDisplay:YES];
            return;
        }
    }
}

- (void)failed:(NSString *)what
{
    [PDSheet showInView:self icon:@"disks" title:what message:NSLocalizedString(@"Check that nothing is using the volume, then try again.", @"")
                  field:nil mono:nil buttons:@[ NSLocalizedString(@"OK", @"") ] styles:@[ @"go" ] done:nil];
}

- (void)unmount:(Volume *)v
{
    if (v->mount == nil || v->boot)
        return;
    if (run(@[ @"/sbin/umount", v->mount ]) != 0)
        [self failed:[NSString stringWithFormat:NSLocalizedString(@"“%@” couldn’t be unmounted.", @""), v->name]];
}

// mounted under /Volumes by its name, with the file system the partition type names
- (void)mount:(Volume *)v
{
    NSDictionary *types = @{ @"ext4": @"ext4", @"FAT32": @"msdos", @"APFS": @"apfs", @"HFS+": @"hfs" };
    NSString *type = types[v->fs], *where = [@"/Volumes" stringByAppendingPathComponent:v->name];

    if (v->mount != nil || type == nil)
        return;
    mkdir("/Volumes", 0755);
    mkdir([where fileSystemRepresentation], 0755);
    if (run(@[ @"/sbin/mount", @"-t", type, [@"/dev/" stringByAppendingString:v->bsd], where ]) != 0) {
        rmdir([where fileSystemRepresentation]);
        [self failed:[NSString stringWithFormat:NSLocalizedString(@"“%@” couldn’t be mounted.", @""), v->name]];
    }
}

- (IBAction)toggleMount:(id)sender
{
    if ([_selected isKindOfClass:[Volume class]]) {
        Volume *v = _selected;
        v->mount != nil ? [self unmount:v] : [self mount:v];
    } else if ([_selected isKindOfClass:[Disk class]]) {
        for (Volume *v in ((Disk *)_selected)->vols)
            [self unmount:v];
    }
    [self reload];
}

- (IBAction)mountVolume:(id)sender
{
    if ([_selected isKindOfClass:[Volume class]])
        [self mount:_selected];
    [self reload];
}

- (IBAction)unmountVolume:(id)sender
{
    if ([_selected isKindOfClass:[Volume class]])
        [self unmount:_selected];
    [self reload];
}

- (IBAction)showInfo:(id)sender
{
    Volume *v = [_selected isKindOfClass:[Volume class]] ? _selected : nil;
    Disk *d = v != nil ? [self diskOf:v] : _selected;
    NSString *mono;

    if (d == nil)
        return;
    mono = [NSString stringWithFormat:@"Device Identifier:  %@\nDevice Node:        /dev/%@\nPart of Whole:      %@\nProtocol:           %@\nFile System:        %@\nMount Point:        %@\nDisk Size:          %@ (%.0f Bytes)\nDevice Block Size:  %u Bytes",
            v ? v->bsd : d->bsd, v ? v->bsd : d->bsd, d->bsd, d->bus, v ? v->fs : @"none", v && v->mount ? v->mount : @"Not mounted",
            PDBytes(v ? v->size : d->size), v ? v->size : d->size, d->blockSize];
    [PDSheet showInView:self icon:@"disks" title:v ? v->name : d->name message:[@"diskutil info " stringByAppendingString:v ? v->bsd : d->bsd]
                  field:nil mono:mono buttons:@[ NSLocalizedString(@"Done", @"") ] styles:@[ @"go" ] done:nil];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    Volume *v = [_selected isKindOfClass:[Volume class]] ? _selected : nil;

    if ([item action] == @selector(mountVolume:))
        return v != nil && v->mount == nil;
    if ([item action] == @selector(unmountVolume:))
        return v != nil && v->mount != nil && !v->boot;
    return [item action] != NULL;
}

@end

@interface DisksController : NSObject {
    NSWindow *_window;
    DisksView *_view;
}
@end

@implementation DisksController

- (void)buildMenu
{
    NSMenu *windowMenu;
    NSMenu *main = PDStandardMenus(NSLocalizedString(@"Disks", @""), &windowMenu);
    NSMenu *file = PDAddMenu(main, NSLocalizedString(@"File", @""));
    NSMenu *disk = PDAddMenu(main, NSLocalizedString(@"Disk", @""));
    NSMenu *view = PDAddMenu(main, NSLocalizedString(@"View", @""));

    [PDAddItem(file, NSLocalizedString(@"New Disk Image…", @""), NULL, @"n", 0) setEnabled:NO];
    [PDAddItem(file, NSLocalizedString(@"Open Disk Image…", @""), NULL, @"o", NSCommandKeyMask | NSAlternateKeyMask) setEnabled:NO];
    PDAddItem(disk, NSLocalizedString(@"Mount", @""), @selector(mountVolume:), @"", 0);
    PDAddItem(disk, NSLocalizedString(@"Unmount", @""), @selector(unmountVolume:), @"e", 0);
    [disk addItem:[NSMenuItem separatorItem]];
    [PDAddItem(disk, NSLocalizedString(@"Check and Repair…", @""), NULL, @"", 0) setEnabled:NO];
    [PDAddItem(disk, NSLocalizedString(@"Partition…", @""), NULL, @"", 0) setEnabled:NO];
    [PDAddItem(disk, NSLocalizedString(@"Erase…", @""), NULL, @"e", NSCommandKeyMask | NSShiftKeyMask) setEnabled:NO];
    [disk addItem:[NSMenuItem separatorItem]];
    PDAddItem(disk, NSLocalizedString(@"Get Info", @""), @selector(showInfo:), @"i", 0);
    [PDAddItem(view, NSLocalizedString(@"Refresh", @""), @selector(refresh:), @"r", 0) setTarget:self];
    [PDAddItem(main, NSLocalizedString(@"Window", @""), NULL, @"", 0) setSubmenu:windowMenu];
    [NSApp setMainMenu:main];
}

- (IBAction)refresh:(id)sender
{
    [_view reload];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self buildMenu];
    _window = [PDMakeWindow(NSMakeSize(880, 560), NSMakeSize(640, 400), 0, self) retain];
    _view = [[DisksView alloc] initWithFrame:[[_window contentView] bounds]];
    [_window setContentView:_view];
    [_window setTitle:NSLocalizedString(@"Disks", @"")];
    [_window makeFirstResponder:_view];
    [_window makeKeyAndOrderFront:nil];
    // disks come and go, the list follows every few seconds
    [NSTimer scheduledTimerWithTimeInterval:3 target:self selector:@selector(refresh:) userInfo:nil repeats:YES];
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
    DisksController *controller = [[DisksController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
