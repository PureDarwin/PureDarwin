// main.m - Activity: the processes the system runs, how busy each core is and where memory goes
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>
#import "PDUtility.h"
#include <libproc.h>
#include <sys/sysctl.h>
#include <sys/proc_info.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pwd.h>
#include <signal.h>

#define ROW_H 22
#define HEAD_H 24
#define SUMMARY_H 190
#define SAMPLES 60
#define REFRESH 2.0
// per-core charts up to this many logical cpus, a sparkline grid past it
#define PER_CORE_MAX 8
// a thread counts as busy at this share of one core, in TH_USAGE_SCALE thousandths
#define BUSY_THREAD 800

@interface Proc : NSObject {
@public
    pid_t pid;
    NSString *name;
    NSString *user;
    NSString *path;
    uint64_t cpuTime;
    double cpu;
    double mem;
    int threads;
    int busyThreads;
    BOOL seen;
}
@end

@implementation Proc

- (void)dealloc
{
    [name release];
    [user release];
    [path release];
    [super dealloc];
}

@end

typedef struct {
    float sys, user;
} CoreSample;

enum { ColName, ColCPU, ColTime, ColThreads, ColPID, ColUser, ColMem };

@interface ActivityView : NSView <NSTextFieldDelegate> {
    NSMutableDictionary *_procs;
    NSMutableArray *_rows;
    Proc *_selected;
    int _sortCol;
    BOOL _ascending;
    BOOL _memoryTab;
    BOOL _mineOnly;
    NSInteger _coreFilter;
    CGFloat _scroll;
    NSTextField *_search;
    NSArray *_buttons;
    PDToolButton *_kill;
    PDToolButton *_info;
    PDSegments *_tabs;
    CoreSample *_hist;
    unsigned _cores;
    natural_t *_lastTicks;
    uint64_t _lastWall;
    NSMutableArray *_coreRects;
    NSTimer *_timer;
}
- (IBAction)quitProcess:(id)sender;
- (IBAction)forceQuitProcess:(id)sender;
- (IBAction)inspectProcess:(id)sender;
- (IBAction)showCPU:(id)sender;
- (IBAction)showMemory:(id)sender;
- (IBAction)toggleMine:(id)sender;
- (void)refresh:(NSTimer *)timer;
@end

static NSString *timeString(uint64_t ns)
{
    double s = ns / 1e9;
    int h = (int)(s / 3600), m = (int)(s / 60) % 60;

    s = fmod(s, 60);
    return h > 0 ? [NSString stringWithFormat:@"%d:%02d:%05.2f", h, m, s] : [NSString stringWithFormat:@"%d:%05.2f", m, s];
}

static NSString *userName(uid_t uid)
{
    static NSMutableDictionary *cache;
    NSString *name;

    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    name = cache[@(uid)];
    if (name == nil) {
        struct passwd *pw = getpwuid(uid);

        name = pw != NULL ? [NSString stringWithUTF8String:pw->pw_name] : [NSString stringWithFormat:@"%u", uid];
        cache[@(uid)] = name;
    }
    return name;
}

// an app's own icon from its bundle, for processes that run out of an .app
static NSImage *iconForPath(NSString *path)
{
    static NSMutableDictionary *cache;
    NSRange r = [path rangeOfString:@".app/"];
    NSString *bundle;
    NSImage *image;

    if (path == nil || r.location == NSNotFound)
        return nil;
    if (cache == nil)
        cache = [[NSMutableDictionary alloc] init];
    bundle = [path substringToIndex:r.location + 4];
    image = cache[bundle];
    if (image == nil) {
        image = [[[NSImage alloc] initWithContentsOfFile:[bundle stringByAppendingPathComponent:@"Contents/Resources/AppIcon.png"]] autorelease];
        cache[bundle] = image != nil ? (id)image : (id)[NSNull null];
    }
    return [image isKindOfClass:[NSImage class]] ? image : nil;
}

static uint64_t nowNanos(void)
{
    static mach_timebase_info_data_t tb;

    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return mach_absolute_time() * tb.numer / tb.denom;
}

@implementation ActivityView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _procs = [[NSMutableDictionary alloc] init];
    _rows = [[NSMutableArray alloc] init];
    _coreRects = [[NSMutableArray alloc] init];
    _sortCol = ColCPU;
    _coreFilter = -1;
    _kill = [PDToolButton buttonWithGlyph:@"stop" label:nil target:self action:@selector(quitProcess:)];
    _info = [PDToolButton buttonWithGlyph:@"info" label:nil target:self action:@selector(inspectProcess:)];
    _buttons = [@[ _kill, _info ] retain];
    _tabs = [[PDSegments alloc] init];
    _tabs->labels = [@[ NSLocalizedString(@"CPU", @""), NSLocalizedString(@"Memory", @"") ] retain];
    _search = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [_search setFont:PDFont(@"Inter-Regular", 12)];
    [_search setBezeled:NO];
    [_search setBordered:NO];
    [_search setDrawsBackground:NO];
    [[_search cell] setPlaceholderString:NSLocalizedString(@"Search", @"")];
    [_search setDelegate:self];
    [self addSubview:_search];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    host_basic_info_data_t info;
    mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
    host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&info, &count);
    _cores = MAX(1, info.logical_cpu);
    _hist = calloc(_cores * SAMPLES, sizeof(CoreSample));
    [self refresh:nil];
    _timer = [[NSTimer scheduledTimerWithTimeInterval:REFRESH target:self selector:@selector(refresh:) userInfo:nil repeats:YES] retain];
    return self;
}

- (void)dealloc
{
    [_timer invalidate];
    [_timer release];
    [_procs release];
    [_rows release];
    [_coreRects release];
    [_buttons release];
    [_tabs release];
    [_search release];
    free(_hist);
    free(_lastTicks);
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

- (void)stop
{
    [_timer invalidate];
}

// per-core ticks since the last refresh, split into system and user time
- (void)sampleCores
{
    processor_info_array_t info;
    mach_msg_type_number_t count;
    natural_t n;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &n, &info, &count) != KERN_SUCCESS)
        return;
    n = MIN(n, _cores);
    memmove(_hist, _hist + _cores, _cores * (SAMPLES - 1) * sizeof(CoreSample));
    for (natural_t c = 0; c < n; c++) {
        natural_t *t = (natural_t *)&info[c * CPU_STATE_MAX];
        CoreSample *s = &_hist[(SAMPLES - 1) * _cores + c];

        if (_lastTicks != NULL) {
            natural_t *o = &_lastTicks[c * CPU_STATE_MAX];
            double user = t[CPU_STATE_USER] - o[CPU_STATE_USER] + t[CPU_STATE_NICE] - o[CPU_STATE_NICE];
            double sys = t[CPU_STATE_SYSTEM] - o[CPU_STATE_SYSTEM];
            double all = user + sys + t[CPU_STATE_IDLE] - o[CPU_STATE_IDLE];

            s->user = all > 0 ? 100 * user / all : 0;
            s->sys = all > 0 ? 100 * sys / all : 0;
        }
    }
    if (_lastTicks == NULL)
        _lastTicks = malloc(n * CPU_STATE_MAX * sizeof(natural_t));
    memcpy(_lastTicks, info, n * CPU_STATE_MAX * sizeof(natural_t));
    vm_deallocate(mach_task_self(), (vm_address_t)info, count * sizeof(integer_t));
}

- (int)busyThreadsOf:(pid_t)pid
{
    uint64_t ids[256];
    int n = proc_pidinfo(pid, PROC_PIDLISTTHREADS, 0, ids, sizeof(ids)) / (int)sizeof(uint64_t), busy = 0;

    for (int i = 0; i < n; i++) {
        struct proc_threadinfo ti;

        if (proc_pidinfo(pid, PROC_PIDTHREADINFO, ids[i], &ti, sizeof(ti)) == sizeof(ti) && ti.pth_cpu_usage >= BUSY_THREAD)
            busy++;
    }
    return busy;
}

- (void)sampleProcesses
{
    int cap = proc_listallpids(NULL, 0) + 32;
    pid_t *pids = calloc(cap, sizeof(pid_t));
    int n = proc_listallpids(pids, cap * sizeof(pid_t));
    uint64_t wall = nowNanos(), dt = _lastWall ? wall - _lastWall : 0;

    for (Proc *p in [_procs allValues])
        p->seen = NO;
    for (int i = 0; i < n; i++) {
        struct proc_taskallinfo ti;
        Proc *p;
        uint64_t t;

        if (proc_pidinfo(pids[i], PROC_PIDTASKALLINFO, 0, &ti, sizeof(ti)) != sizeof(ti))
            continue;
        p = _procs[@(pids[i])];
        if (p == nil) {
            char path[PROC_PIDPATHINFO_MAXSIZE];

            p = [[[Proc alloc] init] autorelease];
            p->pid = pids[i];
            p->name = [[NSString alloc] initWithUTF8String:ti.pbsd.pbi_name[0] ? ti.pbsd.pbi_name : ti.pbsd.pbi_comm];
            p->user = [userName(ti.pbsd.pbi_uid) retain];
            if (proc_pidpath(pids[i], path, sizeof(path)) > 0)
                p->path = [[NSString alloc] initWithUTF8String:path];
            // apps show their bundle's name, the executable is often the same
            if (p->path != nil && [p->path rangeOfString:@".app/"].location != NSNotFound) {
                NSString *app = [[p->path substringToIndex:[p->path rangeOfString:@".app/"].location] lastPathComponent];
                [p->name release];
                p->name = [app retain];
            }
            _procs[@(pids[i])] = p;
            p->cpuTime = ti.ptinfo.pti_total_user + ti.ptinfo.pti_total_system;
        }
        t = ti.ptinfo.pti_total_user + ti.ptinfo.pti_total_system;
        p->cpu = dt > 0 && t >= p->cpuTime ? 100.0 * (t - p->cpuTime) / dt : 0;
        p->cpuTime = t;
        p->mem = ti.ptinfo.pti_resident_size;
        p->threads = ti.ptinfo.pti_threadnum;
        p->busyThreads = _coreFilter >= 0 ? [self busyThreadsOf:pids[i]] : 0;
        p->seen = YES;
    }
    for (NSNumber *k in [_procs allKeys]) {
        if (!((Proc *)_procs[k])->seen) {
            if (_selected == _procs[k])
                _selected = nil;
            [_procs removeObjectForKey:k];
        }
    }
    // the kernel task's time is the whole machine's idle and system time, it is listed without it
    _lastWall = wall;
    free(pids);
}

- (void)sortRows
{
    NSString *q = [[_search stringValue] lowercaseString];

    [_rows removeAllObjects];
    for (Proc *p in [_procs allValues]) {
        if ([q length] > 0 && [[p->name lowercaseString] rangeOfString:q].location == NSNotFound && ![[NSString stringWithFormat:@"%d", p->pid] isEqualToString:q])
            continue;
        if (_mineOnly && ![p->user isEqualToString:NSUserName()])
            continue;
        if (_coreFilter >= 0 && p->busyThreads == 0)
            continue;
        [_rows addObject:p];
    }
    [_rows sortUsingComparator:^NSComparisonResult(Proc *a, Proc *b) {
        NSComparisonResult r;

        switch (_sortCol) {
        case ColName: r = [a->name caseInsensitiveCompare:b->name]; break;
        case ColUser: r = [a->user compare:b->user]; break;
        case ColPID: r = a->pid < b->pid ? NSOrderedAscending : a->pid > b->pid ? NSOrderedDescending : NSOrderedSame; break;
        case ColThreads: r = a->threads < b->threads ? NSOrderedAscending : a->threads > b->threads ? NSOrderedDescending : NSOrderedSame; break;
        case ColTime: r = a->cpuTime < b->cpuTime ? NSOrderedAscending : a->cpuTime > b->cpuTime ? NSOrderedDescending : NSOrderedSame; break;
        case ColMem: r = a->mem < b->mem ? NSOrderedAscending : a->mem > b->mem ? NSOrderedDescending : NSOrderedSame; break;
        default: r = a->cpu < b->cpu ? NSOrderedAscending : a->cpu > b->cpu ? NSOrderedDescending : NSOrderedSame; break;
        }
        if (r == NSOrderedSame)
            r = a->pid < b->pid ? NSOrderedAscending : NSOrderedDescending;
        return _ascending ? r : -r;
    }];
}

- (void)refresh:(NSTimer *)timer
{
    [self sampleCores];
    [self sampleProcesses];
    [self sortRows];
    [self setNeedsDisplay:YES];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    [self sortRows];
    _scroll = 0;
    [self setNeedsDisplay:YES];
}

- (NSArray *)columns
{
    // key, title, share of the width, right aligned
    if (_memoryTab)
        return @[ @[ @(ColName), NSLocalizedString(@"Process Name", @""), @.42, @NO ], @[ @(ColMem), NSLocalizedString(@"Memory", @""), @.14, @YES ],
                  @[ @(ColThreads), NSLocalizedString(@"Threads", @""), @.11, @YES ], @[ @(ColPID), NSLocalizedString(@"PID", @""), @.1, @YES ],
                  @[ @(ColUser), NSLocalizedString(@"User", @""), @.23, @NO ] ];
    return @[ @[ @(ColName), NSLocalizedString(@"Process Name", @""), @.38, @NO ], @[ @(ColCPU), NSLocalizedString(@"% CPU", @""), @.09, @YES ],
              @[ @(ColTime), NSLocalizedString(@"CPU Time", @""), @.12, @YES ], @[ @(ColThreads), NSLocalizedString(@"Threads", @""), @.1, @YES ],
              @[ @(ColPID), NSLocalizedString(@"PID", @""), @.09, @YES ], @[ @(ColUser), NSLocalizedString(@"User", @""), @.22, @NO ] ];
}

- (NSRect)tableRect
{
    NSRect b = [self bounds];

    return NSMakeRect(0, PD_TOOLBAR_H + 1, NSWidth(b), NSHeight(b) - PD_TOOLBAR_H - 1 - SUMMARY_H);
}

- (NSString *)cell:(Proc *)p column:(int)col
{
    switch (col) {
    case ColName: return p->name;
    case ColCPU: return [NSString stringWithFormat:@"%.1f", p->cpu];
    case ColTime: return timeString(p->cpuTime);
    case ColThreads: return _coreFilter >= 0 ? [NSString stringWithFormat:@"%d / %d", p->busyThreads, p->threads] : [NSString stringWithFormat:@"%d", p->threads];
    case ColPID: return [NSString stringWithFormat:@"%d", p->pid];
    case ColUser: return p->user;
    default: return PDBytes(p->mem);
    }
}

- (void)drawToolbarIn:(NSRect)b
{
    NSString *what = _coreFilter >= 0 ? [NSString stringWithFormat:NSLocalizedString(@"Busy threads, Core %ld", @""), (long)_coreFilter]
                                      : _mineOnly ? NSLocalizedString(@"My Processes", @"") : NSLocalizedString(@"All Processes", @"");
    NSRect find = NSMakeRect(NSWidth(b) - 180, 9, 170, 26);

    PDDrawToolbar(self, NSMakeRect(0, 0, NSWidth(b), PD_TOOLBAR_H));
    PDDrawTitle(NSLocalizedString(@"Activity", @""), [NSString stringWithFormat:@"%@, %lu", what, (unsigned long)[_rows count]],
                NSMakePoint(PD_LIGHTS_W, PD_TOOLBAR_H / 2), 236, NO);
    _kill->enabled = _info->enabled = _selected != nil;
    PDLayoutButtons(_buttons, PD_LIGHTS_W + 320, PD_TOOLBAR_H, NO);
    _tabs->selected = _memoryTab ? 1 : 0;
    [_tabs drawCenteredIn:NSMakeRect(MAX(NSWidth(b) / 2 + 90, PD_LIGHTS_W + 400), 0, 0, PD_TOOLBAR_H)];
    PDFillRounded(find, 6, PDAlpha(PDColor(0x000000, 0xffffff), .06));
    PDDrawGlyph(@"search", NSMakeRect(NSMinX(find) + 8, NSMidY(find) - 6, 12, 12), PDColor(0x8a8a8a, 0x8a8a8a));
    [_search setFrame:NSMakeRect(NSMinX(find) + 26, NSMinY(find) + 5, NSWidth(find) - 32, 17)];
    if ([[_search stringValue] length] == 0 && [[self window] firstResponder] != [_search currentEditor])
        [NSLocalizedString(@"Search", @"") drawAtPoint:NSMakePoint(NSMinX(find) + 28, NSMinY(find) + 5) withAttributes:PDText(PDFont(@"Inter-Regular", 12), PDColor(0x9a9a9a, 0x8a8a8a))];
}

- (void)drawTable
{
    NSRect t = [self tableRect];
    NSArray *cols = [self columns];
    NSDictionary *head = PDText(PDFont(@"Inter-Medium", 11), PDColor(0x6e6e6e, 0x9a9a9a));
    NSDictionary *headOn = PDText(PDFont(@"Inter-SemiBold", 11), PDColor(0x1b1b1b, 0xeeeeee));
    NSFont *f = PDFont(@"Inter-Regular", 12);
    CGFloat x, y = NSMinY(t) + HEAD_H - _scroll;

    [PDColor(0xffffff, 0x1f1f21) setFill];
    NSRectFill(t);
    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:NSMakeRect(0, NSMinY(t) + HEAD_H, NSWidth(t), NSHeight(t) - HEAD_H)];
    for (NSUInteger i = 0; i < [_rows count]; i++, y += ROW_H) {
        Proc *p = _rows[i];
        BOOL sel = p == _selected;
        NSColor *ink = sel ? [NSColor whiteColor] : PDColor(0x1b1b1b, 0xe6e6e6);

        if (y + ROW_H < NSMinY(t) + HEAD_H || y > NSMaxY(t))
            continue;
        if (sel)
            [[NSColor controlAccentColor] setFill];
        else
            [(i % 2 ? PDColor(0xf7f7f7, 0x252527) : PDColor(0xffffff, 0x1f1f21)) setFill];
        NSRectFill(NSMakeRect(0, y, NSWidth(t), ROW_H));
        x = 10;
        for (NSArray *c in cols) {
            CGFloat w = floor([c[2] doubleValue] * (NSWidth(t) - 20));
            int key = [c[0] intValue];
            NSDictionary *attrs = PDText(f, ink);
            NSString *text = [self cell:p column:key];
            CGFloat tx = x + 4;

            if (key == ColName) {
                NSImage *icon = iconForPath(p->path);
                NSRect ir = NSMakeRect(x, y + 3, 16, 16);

                if (icon != nil)
                    [icon drawInRect:ir fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
                else
                    PDFillRounded(ir, 4, PDColor(0xd8d8d8, 0x48484a));
                tx = x + 22;
            }
            text = PDFitted(text, attrs, w - (tx - x) - 8);
            if ([c[3] boolValue])
                tx = x + w - 8 - [text sizeWithAttributes:attrs].width;
            [text drawAtPoint:NSMakePoint(tx, y + 3) withAttributes:attrs];
            x += w;
        }
    }
    [NSGraphicsContext restoreGraphicsState];

    // the header stays put over the rows
    [PDColor(0xfafafa, 0x262628) setFill];
    NSRectFill(NSMakeRect(0, NSMinY(t), NSWidth(t), HEAD_H));
    [PDColor(0xe2e2e2, 0x141414) setFill];
    NSRectFill(NSMakeRect(0, NSMinY(t) + HEAD_H - 1, NSWidth(t), 1));
    x = 10;
    for (NSArray *c in cols) {
        CGFloat w = floor([c[2] doubleValue] * (NSWidth(t) - 20));
        BOOL on = [c[0] intValue] == _sortCol;
        NSString *title = on ? [c[1] stringByAppendingString:_ascending ? @" ▴" : @" ▾"] : c[1];
        NSDictionary *a = on ? headOn : head;
        CGFloat tx = [c[3] boolValue] ? x + w - 8 - [title sizeWithAttributes:a].width : x + 4;

        [title drawAtPoint:NSMakePoint(tx, NSMinY(t) + 5) withAttributes:a];
        x += w;
    }
}

- (void)kv:(NSString *)k value:(NSString *)v color:(NSColor *)c at:(NSPoint)p width:(CGFloat)w
{
    NSDictionary *ka = PDText(PDFont(@"Inter-Regular", 11.5), PDColor(0x6e6e6e, 0x9a9a9a));
    NSDictionary *va = PDText(PDFont(@"Inter-SemiBold", 11.5), c != nil ? c : PDColor(0x1b1b1b, 0xeeeeee));
    NSSize vs = [v sizeWithAttributes:va];

    [k drawAtPoint:p withAttributes:ka];
    [v drawAtPoint:NSMakePoint(p.x + w - vs.width, p.y) withAttributes:va];
}

static void stackArea(CoreSample *samples, unsigned stride, unsigned core, NSRect r, CGFloat max)
{
    NSBezierPath *all = [NSBezierPath bezierPath], *sys = [NSBezierPath bezierPath];
    CGFloat step = NSWidth(r) / (SAMPLES - 1);

    for (int pass = 0; pass < 2; pass++) {
        NSBezierPath *p = pass ? sys : all;

        [p moveToPoint:NSMakePoint(NSMinX(r), NSMaxY(r))];
        for (int i = 0; i < SAMPLES; i++) {
            CoreSample s = samples[i * stride + core];
            CGFloat v = pass ? s.sys : s.sys + s.user;

            [p lineToPoint:NSMakePoint(NSMinX(r) + i * step, NSMaxY(r) - MIN(v, max) / max * NSHeight(r))];
        }
        [p lineToPoint:NSMakePoint(NSMaxX(r), NSMaxY(r))];
        [p closePath];
    }
    [PDAlpha(PDColor(0x2f6fdb, 0x6ea8ff), .28) setFill];
    [all fill];
    [PDColor(0x2f6fdb, 0x6ea8ff) setStroke];
    [all setLineWidth:1];
    [all stroke];
    [PDAlpha(PDColor(0xe5483f, 0xff7b72), .35) setFill];
    [sys fill];
    [PDColor(0xe5483f, 0xff7b72) setStroke];
    [sys setLineWidth:1];
    [sys stroke];
}

- (void)drawCoresIn:(NSRect)r
{
    NSDictionary *small = PDText(PDFont(@"Inter-Regular", 10), PDColor(0x8a8a8a, 0x8a8a8a));
    NSDictionary *pct = PDText(PDFont(@"Inter-SemiBold", 10), PDColor(0x1b1b1b, 0xeeeeee));
    BOOL grid = _cores > PER_CORE_MAX;
    unsigned cols = grid ? (_cores > 16 ? 8 : 4) : (_cores <= 4 ? 2 : 4);
    unsigned rows = (_cores + cols - 1) / cols;
    CGFloat lab = grid ? 18 : 0, gap = 6;
    CGFloat w = (NSWidth(r) - lab - (cols - 1) * gap) / cols, h = (NSHeight(r) - (rows - 1) * gap) / rows;

    [_coreRects removeAllObjects];
    for (unsigned c = 0; c < _cores; c++) {
        CoreSample now = _hist[(SAMPLES - 1) * _cores + c];
        NSRect tile = NSMakeRect(floor(NSMinX(r) + lab + (c % cols) * (w + gap)), floor(NSMinY(r) + (c / cols) * (h + gap)), floor(w), floor(h));
        NSString *load = [NSString stringWithFormat:grid ? @"%.0f" : @"%.0f%%", now.sys + now.user];
        NSRect chart = grid ? tile : NSMakeRect(NSMinX(tile), NSMinY(tile) + 15, NSWidth(tile), NSHeight(tile) - 15);

        if (grid && c % cols == 0) {
            NSString *n = [NSString stringWithFormat:@"%u", c];
            NSSize ns = [n sizeWithAttributes:small];
            [n drawAtPoint:NSMakePoint(NSMinX(r) + lab - 4 - ns.width, NSMidY(tile) - ns.height / 2) withAttributes:small];
        }
        PDFillRounded(chart, 3, PDColor(0xffffff, 0x1b1b1d));
        [NSGraphicsContext saveGraphicsState];
        [[NSBezierPath bezierPathWithRoundedRect:chart xRadius:3 yRadius:3] addClip];
        stackArea(_hist, _cores, c, NSInsetRect(chart, 0, 1), 100);
        [NSGraphicsContext restoreGraphicsState];
        if (grid) {
            NSSize ls = [load sizeWithAttributes:pct];
            [load drawAtPoint:NSMakePoint(NSMaxX(tile) - 3 - ls.width, NSMinY(tile)) withAttributes:pct];
        } else {
            NSSize ls = [load sizeWithAttributes:pct];
            [[NSString stringWithFormat:NSLocalizedString(@"Core %u", @""), c] drawAtPoint:NSMakePoint(NSMinX(tile), NSMinY(tile)) withAttributes:small];
            [load drawAtPoint:NSMakePoint(NSMaxX(tile) - ls.width, NSMinY(tile)) withAttributes:pct];
        }
        if ((NSInteger)c == _coreFilter) {
            [[NSColor controlAccentColor] setStroke];
            NSBezierPath *ring = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(tile, -2, -2) xRadius:4 yRadius:4];
            [ring setLineWidth:2];
            [ring stroke];
        }
        [_coreRects addObject:[NSValue valueWithRect:NSInsetRect(tile, -2, -2)]];
    }
}

- (void)drawSummary
{
    NSRect b = [self bounds];
    NSRect s = NSMakeRect(0, NSHeight(b) - SUMMARY_H, NSWidth(b), SUMMARY_H);
    CGFloat c1 = floor(NSWidth(b) * .29), c3 = floor(NSWidth(b) * .29), c2 = NSWidth(b) - c1 - c3;
    NSDictionary *h5 = PDText(PDFont(@"Inter-Regular", 11), PDColor(0x6e6e6e, 0x9a9a9a));
    double sys = 0, user = 0;
    int threads = 0;

    [PDColor(0xfafafa, 0x262628) setFill];
    NSRectFill(s);
    [PDColor(0xe2e2e2, 0x141414) setFill];
    NSRectFill(NSMakeRect(0, NSMinY(s), NSWidth(b), 1));
    [PDColor(0xe8e8e8, 0x333333) setFill];
    NSRectFill(NSMakeRect(c1, NSMinY(s) + 1, 1, SUMMARY_H));
    NSRectFill(NSMakeRect(c1 + c2, NSMinY(s) + 1, 1, SUMMARY_H));

    for (unsigned c = 0; c < _cores; c++) {
        sys += _hist[(SAMPLES - 1) * _cores + c].sys / _cores;
        user += _hist[(SAMPLES - 1) * _cores + c].user / _cores;
    }
    for (Proc *p in [_procs allValues])
        threads += p->threads;

    if (!_memoryTab) {
        CGFloat y = NSMinY(s) + 12;
        double load[3];
        char cpu[128] = "";
        size_t len = sizeof(cpu);

        [self kv:NSLocalizedString(@"System", @"") value:[NSString stringWithFormat:@"%.1f%%", sys] color:PDColor(0xe5483f, 0xff7b72) at:NSMakePoint(16, y) width:c1 - 32];
        [self kv:NSLocalizedString(@"User", @"") value:[NSString stringWithFormat:@"%.1f%%", user] color:PDColor(0x2f6fdb, 0x6ea8ff) at:NSMakePoint(16, y + 19) width:c1 - 32];
        [self kv:NSLocalizedString(@"Idle", @"") value:[NSString stringWithFormat:@"%.1f%%", MAX(0, 100 - sys - user)] color:nil at:NSMakePoint(16, y + 38) width:c1 - 32];
        [self kv:NSLocalizedString(@"Threads", @"") value:[NSString stringWithFormat:@"%d", threads] color:nil at:NSMakePoint(16, y + 57) width:c1 - 32];
        [self kv:NSLocalizedString(@"Processes", @"") value:[NSString stringWithFormat:@"%lu", (unsigned long)[_procs count]] color:nil at:NSMakePoint(16, y + 76) width:c1 - 32];

        [NSLocalizedString(@"CPU History", @"") drawAtPoint:NSMakePoint(c1 + 16, y) withAttributes:h5];
        [self drawLegendAt:NSMakePoint(c1 + c2 - 16, y)];
        [self drawCoresIn:NSMakeRect(c1 + 16, y + 22, c2 - 32, SUMMARY_H - 46)];

        sysctlbyname("machdep.cpu.brand_string", cpu, &len, NULL, 0);
        getloadavg(load, 3);
        [NSLocalizedString(@"Processor", @"") drawAtPoint:NSMakePoint(c1 + c2 + 16, y) withAttributes:h5];
        [PDFitted([NSString stringWithUTF8String:cpu], PDText(PDFont(@"Inter-Medium", 11.5), PDColor(0x1b1b1b, 0xeeeeee)), c3 - 32)
            drawAtPoint:NSMakePoint(c1 + c2 + 16, y + 19) withAttributes:PDText(PDFont(@"Inter-Medium", 11.5), PDColor(0x1b1b1b, 0xeeeeee))];
        [self kv:NSLocalizedString(@"Logical CPUs", @"") value:[NSString stringWithFormat:@"%u", _cores] color:nil at:NSMakePoint(c1 + c2 + 16, y + 38) width:c3 - 32];
        [self kv:NSLocalizedString(@"Load Average", @"") value:[NSString stringWithFormat:@"%.2f %.2f %.2f", load[0], load[1], load[2]] color:nil
              at:NSMakePoint(c1 + c2 + 16, y + 57) width:c3 - 32];
    } else {
        [self drawMemoryIn:s columns:c1 :c2 :c3];
    }
}

- (void)drawLegendAt:(NSPoint)right
{
    NSDictionary *a = PDText(PDFont(@"Inter-Regular", 10.5), PDColor(0x8a8a8a, 0x8a8a8a));
    NSString *u = NSLocalizedString(@"User", @""), *s = NSLocalizedString(@"System", @"");
    CGFloat x = right.x - [u sizeWithAttributes:a].width;

    [u drawAtPoint:NSMakePoint(x, right.y) withAttributes:a];
    PDFillRounded(NSMakeRect(x - 12, right.y + 4, 8, 8), 2, PDColor(0x2f6fdb, 0x6ea8ff));
    x -= 22 + [s sizeWithAttributes:a].width;
    [s drawAtPoint:NSMakePoint(x, right.y) withAttributes:a];
    PDFillRounded(NSMakeRect(x - 12, right.y + 4, 8, 8), 2, PDColor(0xe5483f, 0xff7b72));
}

- (void)drawMemoryIn:(NSRect)s columns:(CGFloat)c1 :(CGFloat)c2 :(CGFloat)c3
{
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    uint64_t total = 0;
    size_t len = sizeof(total);
    struct xsw_usage swap = { 0 };
    size_t slen = sizeof(swap);
    vm_size_t page = vm_kernel_page_size;
    CGFloat y = NSMinY(s) + 12;
    NSDictionary *h5 = PDText(PDFont(@"Inter-Regular", 11), PDColor(0x6e6e6e, 0x9a9a9a));

    sysctlbyname("hw.memsize", &total, &len, NULL, 0);
    sysctlbyname("vm.swapusage", &swap, &slen, NULL, 0);
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) != KERN_SUCCESS)
        memset(&vm, 0, sizeof(vm));
    double wired = (double)vm.wire_count * page, comp = (double)vm.compressor_page_count * page;
    double app = (double)(vm.internal_page_count - vm.purgeable_count) * page, cached = (double)(vm.external_page_count + vm.purgeable_count) * page;
    double used = app + wired + comp, free_ = MAX(0, (double)total - used - cached);

    [self kv:NSLocalizedString(@"Physical Memory", @"") value:PDBytes(total) color:nil at:NSMakePoint(16, y) width:c1 - 32];
    [self kv:NSLocalizedString(@"Memory Used", @"") value:PDBytes(used) color:nil at:NSMakePoint(16, y + 19) width:c1 - 32];
    [self kv:NSLocalizedString(@"Cached Files", @"") value:PDBytes(cached) color:nil at:NSMakePoint(16, y + 38) width:c1 - 32];
    [self kv:NSLocalizedString(@"Swap Used", @"") value:PDBytes(swap.xsu_used) color:nil at:NSMakePoint(16, y + 57) width:c1 - 32];

    [NSLocalizedString(@"Memory", @"") drawAtPoint:NSMakePoint(c1 + 16, y) withAttributes:h5];
    NSRect bar = NSMakeRect(c1 + 16, y + 22, c2 - 32, 8);
    double parts[4] = { app, wired, comp, cached };
    unsigned colors[4][2] = { { 0x2f6fdb, 0x6ea8ff }, { 0xe5483f, 0xff7b72 }, { 0xb98a74, 0xdba78c }, { 0x9ab8e8, 0x5d7fb5 } };
    CGFloat x = NSMinX(bar);
    PDFillRounded(bar, 4, PDColor(0xe6e6e6, 0x3a3a3c));
    [NSGraphicsContext saveGraphicsState];
    [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:4 yRadius:4] addClip];
    for (int i = 0; i < 4; i++) {
        CGFloat w = total > 0 ? NSWidth(bar) * parts[i] / total : 0;
        [PDColor(colors[i][0], colors[i][1]) setFill];
        NSRectFill(NSMakeRect(x, NSMinY(bar), w, NSHeight(bar)));
        x += w;
    }
    [NSGraphicsContext restoreGraphicsState];
    [self kv:NSLocalizedString(@"App Memory", @"") value:PDBytes(app) color:PDColor(0x2f6fdb, 0x6ea8ff) at:NSMakePoint(c1 + 16, y + 40) width:c2 - 32];
    [self kv:NSLocalizedString(@"Wired", @"") value:PDBytes(wired) color:PDColor(0xe5483f, 0xff7b72) at:NSMakePoint(c1 + 16, y + 59) width:c2 - 32];
    [self kv:NSLocalizedString(@"Compressed", @"") value:PDBytes(comp) color:PDColor(0x8c5a40, 0xdba78c) at:NSMakePoint(c1 + 16, y + 78) width:c2 - 32];

    [self kv:NSLocalizedString(@"Free", @"") value:PDBytes(free_) color:nil at:NSMakePoint(c1 + c2 + 16, y) width:c3 - 32];
    [self kv:NSLocalizedString(@"Page Size", @"") value:[NSString stringWithFormat:@"%lu KB", (unsigned long)page / 1024] color:nil at:NSMakePoint(c1 + c2 + 16, y + 19) width:c3 - 32];
    [self kv:NSLocalizedString(@"Pageins", @"") value:[NSString stringWithFormat:@"%llu", vm.pageins] color:nil at:NSMakePoint(c1 + c2 + 16, y + 38) width:c3 - 32];
    [self kv:NSLocalizedString(@"Pageouts", @"") value:[NSString stringWithFormat:@"%llu", vm.pageouts] color:nil at:NSMakePoint(c1 + c2 + 16, y + 57) width:c3 - 32];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];

    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:8 yRadius:8] addClip];
    [self drawToolbarIn:b];
    [self drawTable];
    [self drawSummary];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSRect t = [self tableRect];
    NSInteger seg;

    if (PDClickButtons(_buttons, p))
        return;
    if ((seg = [_tabs segmentAt:p]) >= 0) {
        _memoryTab = seg == 1;
        if (_memoryTab && _sortCol == ColCPU)
            _sortCol = ColMem;
        else if (!_memoryTab && _sortCol == ColMem)
            _sortCol = ColCPU;
        [self setNeedsDisplay:YES];
        return;
    }
    if (p.y < PD_TOOLBAR_H) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    if (p.y > NSMaxY(t)) {
        // a core picks the processes with a thread near 100% of one, the same core again shows everything
        for (NSUInteger c = 0; c < [_coreRects count] && !_memoryTab; c++) {
            if (NSPointInRect(p, [_coreRects[c] rectValue])) {
                _coreFilter = _coreFilter == (NSInteger)c ? -1 : (NSInteger)c;
                [self refresh:nil];
                return;
            }
        }
        return;
    }
    if (p.y < NSMinY(t) + HEAD_H) {
        CGFloat x = 10;
        for (NSArray *c in [self columns]) {
            CGFloat w = floor([c[2] doubleValue] * (NSWidth(t) - 20));
            if (p.x >= x && p.x < x + w) {
                int key = [c[0] intValue];
                _ascending = key == _sortCol ? !_ascending : (key == ColName || key == ColUser);
                _sortCol = key;
                [self sortRows];
                [self setNeedsDisplay:YES];
                return;
            }
            x += w;
        }
        return;
    }
    NSInteger i = (NSInteger)floor((p.y - NSMinY(t) - HEAD_H + _scroll) / ROW_H);
    _selected = i >= 0 && i < (NSInteger)[_rows count] ? _rows[i] : nil;
    [self setNeedsDisplay:YES];
    if (_selected != nil && [event clickCount] == 2)
        [self inspectProcess:nil];
}

- (void)scrollWheel:(NSEvent *)event
{
    CGFloat max = MAX(0, [_rows count] * ROW_H - (NSHeight([self tableRect]) - HEAD_H));

    _scroll = MIN(MAX(_scroll - [event deltaY] * ROW_H * 2, 0), max);
    [self setNeedsDisplay:YES];
}

- (void)quitWithSignal:(int)sig
{
    Proc *p = _selected;

    if (p == nil)
        return;
    if (p->pid <= 1) {
        [PDSheet showInView:self icon:@"activity" title:[NSString stringWithFormat:NSLocalizedString(@"%@ can’t be quit", @""), p->name]
                    message:NSLocalizedString(@"It is part of the system. Restart instead from the PureDarwin menu.", @"")
                      field:nil mono:nil buttons:@[ NSLocalizedString(@"OK", @"") ] styles:@[ @"go" ] done:nil];
        return;
    }
    pid_t pid = p->pid;
    [PDSheet showInView:self icon:@"activity" title:[NSString stringWithFormat:NSLocalizedString(@"Quit “%@”?", @""), p->name]
                message:p->path == nil || [p->path rangeOfString:@".app/"].location == NSNotFound
                            ? [NSString stringWithFormat:NSLocalizedString(@"launchd will start %@ again if its job keeps it alive.", @""), p->name]
                            : NSLocalizedString(@"Unsaved changes in this app may be lost.", @"")
                  field:nil mono:nil
                buttons:@[ NSLocalizedString(@"Cancel", @""), NSLocalizedString(@"Force Quit", @""), NSLocalizedString(@"Quit", @"") ]
                 styles:@[ @"", @"danger", @"go" ]
                   done:^(NSInteger i, NSString *field) {
                       if (i == 1)
                           kill(pid, SIGKILL);
                       else if (i == 2)
                           kill(pid, SIGTERM);
                       if (i > 0)
                           [self performSelector:@selector(refresh:) withObject:nil afterDelay:.3];
                   }];
}

- (IBAction)quitProcess:(id)sender
{
    [self quitWithSignal:SIGTERM];
}

- (IBAction)forceQuitProcess:(id)sender
{
    [self quitWithSignal:SIGKILL];
}

- (IBAction)inspectProcess:(id)sender
{
    Proc *p = _selected;
    struct proc_bsdinfo bi;
    NSString *mono;

    if (p == nil)
        return;
    memset(&bi, 0, sizeof(bi));
    proc_pidinfo(p->pid, PROC_PIDTBSDINFO, 0, &bi, sizeof(bi));
    mono = [NSString stringWithFormat:@"%@\nparent      %d\nthreads     %d\nresident    %@\ncpu time    %@\nuser        %@",
            p->path != nil ? p->path : p->name, bi.pbi_ppid, p->threads, PDBytes(p->mem), timeString(p->cpuTime), p->user];
    [PDSheet showInView:self icon:@"activity" title:[NSString stringWithFormat:@"%@ (%d)", p->name, p->pid]
                message:[NSString stringWithFormat:NSLocalizedString(@"Owned by %@, %d threads, %@ resident.", @""), p->user, p->threads, PDBytes(p->mem)]
                  field:nil mono:mono buttons:@[ NSLocalizedString(@"Done", @"") ] styles:@[ @"go" ] done:nil];
}

- (IBAction)showCPU:(id)sender
{
    _memoryTab = NO;
    if (_sortCol == ColMem)
        _sortCol = ColCPU;
    [self setNeedsDisplay:YES];
}

- (IBAction)showMemory:(id)sender
{
    _memoryTab = YES;
    if (_sortCol == ColCPU)
        _sortCol = ColMem;
    [self setNeedsDisplay:YES];
}

- (IBAction)toggleMine:(id)sender
{
    _mineOnly = !_mineOnly;
    [self sortRows];
    [self setNeedsDisplay:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    SEL a = [item action];

    if (a == @selector(quitProcess:) || a == @selector(forceQuitProcess:) || a == @selector(inspectProcess:))
        return _selected != nil;
    if (a == @selector(toggleMine:))
        [item setState:_mineOnly ? NSOnState : NSOffState];
    if (a == @selector(showCPU:))
        [item setState:_memoryTab ? NSOffState : NSOnState];
    if (a == @selector(showMemory:))
        [item setState:_memoryTab ? NSOnState : NSOffState];
    return YES;
}

@end

@interface ActivityController : NSObject {
    NSWindow *_window;
    ActivityView *_view;
}
@end

@implementation ActivityController

- (void)buildMenu
{
    NSMenu *windowMenu;
    NSMenu *main = PDStandardMenus(NSLocalizedString(@"Activity", @""), &windowMenu);
    NSMenu *view = PDAddMenu(main, NSLocalizedString(@"View", @""));
    NSMenu *proc = PDAddMenu(main, NSLocalizedString(@"Process", @""));

    PDAddItem(view, NSLocalizedString(@"CPU", @""), @selector(showCPU:), @"1", 0);
    PDAddItem(view, NSLocalizedString(@"Memory", @""), @selector(showMemory:), @"2", 0);
    [view addItem:[NSMenuItem separatorItem]];
    PDAddItem(view, NSLocalizedString(@"My Processes Only", @""), @selector(toggleMine:), @"", 0);
    PDAddItem(proc, NSLocalizedString(@"Quit Process…", @""), @selector(quitProcess:), @"q", NSCommandKeyMask | NSAlternateKeyMask);
    PDAddItem(proc, NSLocalizedString(@"Force Quit Process…", @""), @selector(forceQuitProcess:), @"", 0);
    [proc addItem:[NSMenuItem separatorItem]];
    PDAddItem(proc, NSLocalizedString(@"Inspect", @""), @selector(inspectProcess:), @"i", 0);
    [PDAddItem(main, NSLocalizedString(@"Window", @""), NULL, @"", 0) setSubmenu:windowMenu];
    [NSApp setMainMenu:main];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self buildMenu];
    _window = [PDMakeWindow(NSMakeSize(820, 560), NSMakeSize(600, 420), -40, self) retain];
    _view = [[ActivityView alloc] initWithFrame:[[_window contentView] bounds]];
    [_window setContentView:_view];
    [_window setTitle:NSLocalizedString(@"Activity", @"")];
    [_window makeFirstResponder:_view];
    [_window makeKeyAndOrderFront:nil];
}

- (void)windowWillClose:(NSNotification *)note
{
    [_view stop];
    [NSApp terminate:self];
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [NSApplication sharedApplication];
    ActivityController *controller = [[ActivityController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
