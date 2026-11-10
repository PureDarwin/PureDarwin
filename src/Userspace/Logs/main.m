// main.m - Logs: the kernel buffer, what launchd jobs write to /var/log and crash reports, followed live
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/AppKit.h>
#import "PDUtility.h"
#include <sys/stat.h>

#define SIDE_W 210
#define LINE_H 19
#define MAX_LINES 5000
#define POLL 1.0

int proc_kmsgbuf(void *buffer, uint32_t buffersize);

enum { LevelInfo, LevelWarn, LevelError };

@interface LogLine : NSObject {
@public
    NSString *time;
    NSString *proc;
    NSString *msg;
    NSString *source;
    int level;
    NSTimeInterval added;
}
@end

@implementation LogLine

- (void)dealloc
{
    [time release];
    [proc release];
    [msg release];
    [source release];
    [super dealloc];
}

@end

// a source the sidebar lists: a file followed from where it was last read, or the kernel buffer
@interface LogSource : NSObject {
@public
    NSString *key;
    NSString *title;
    NSString *path;
    NSString *group;
    off_t offset;
    NSString *partial;
}
@end

@implementation LogSource

- (void)dealloc
{
    [key release];
    [title release];
    [path release];
    [group release];
    [partial release];
    [super dealloc];
}

@end

@interface LogsView : NSView <NSTextFieldDelegate> {
    NSMutableArray *_sources;
    NSMutableArray *_lines;
    NSMutableArray *_shown;
    NSString *_selected;
    NSTextField *_filter;
    NSArray *_buttons;
    PDToolButton *_pause;
    BOOL _paused;
    BOOL _problemsOnly;
    CGFloat _scroll;
    BOOL _follow;
    NSString *_kernelSeen;
    NSTimer *_timer;
}
- (IBAction)togglePause:(id)sender;
- (IBAction)clear:(id)sender;
- (IBAction)toggleProblems:(id)sender;
- (IBAction)find:(id)sender;
@end

static NSString *nowStamp(void)
{
    NSDateFormatter *f = [[[NSDateFormatter alloc] init] autorelease];

    [f setDateFormat:@"HH:mm:ss.SSS"];
    return [f stringFromDate:[NSDate date]];
}

// warnings and errors by the words PureDarwin's daemons and the kernel use for them
static int levelOf(NSString *msg)
{
    NSString *l = [msg lowercaseString];

    for (NSString *w in @[ @"error", @"fail", @"panic", @"sigbus", @"sigsegv", @"fault", @"denied", @"cannot", @"can't" ]) {
        if ([l rangeOfString:w].location != NSNotFound)
            return LevelError;
    }
    for (NSString *w in @[ @"warn", @"timed out", @"timeout", @"busy services", @"retry", @"deprecated" ]) {
        if ([l rangeOfString:w].location != NSNotFound)
            return LevelWarn;
    }
    return LevelInfo;
}

@implementation LogsView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _sources = [[NSMutableArray alloc] init];
    _lines = [[NSMutableArray alloc] init];
    _shown = [[NSMutableArray alloc] init];
    _selected = @"all";
    _follow = YES;
    _pause = [PDToolButton buttonWithGlyph:@"pause" label:nil target:self action:@selector(togglePause:)];
    _buttons = [@[ _pause, [PDToolButton buttonWithGlyph:@"clear" label:nil target:self action:@selector(clear:)] ] retain];
    _filter = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [_filter setFont:PDFont(@"Inter-Regular", 12)];
    [_filter setBezeled:NO];
    [_filter setBordered:NO];
    [_filter setDrawsBackground:NO];
    [[_filter cell] setPlaceholderString:NSLocalizedString(@"Filter", @"")];
    [_filter setDelegate:self];
    [self addSubview:_filter];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self findSources];
    [self poll:nil];
    _timer = [[NSTimer scheduledTimerWithTimeInterval:POLL target:self selector:@selector(poll:) userInfo:nil repeats:YES] retain];
    return self;
}

- (void)dealloc
{
    [_timer invalidate];
    [_timer release];
    [_sources release];
    [_lines release];
    [_shown release];
    [_buttons release];
    [_filter release];
    [_kernelSeen release];
    [super dealloc];
}

- (void)stop
{
    [_timer invalidate];
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)addSource:(NSString *)key title:(NSString *)title path:(NSString *)path group:(NSString *)group
{
    LogSource *s = [[[LogSource alloc] init] autorelease];
    struct stat st;

    s->key = [key copy];
    s->title = [title copy];
    s->path = [path copy];
    s->group = [group copy];
    // a file starts from its last 64 KB, older lines are what the file is for
    if (path != nil && stat([path fileSystemRepresentation], &st) == 0)
        s->offset = MAX(0, st.st_size - 65536);
    [_sources addObject:s];
}

- (void)findSources
{
    NSFileManager *fm = [NSFileManager defaultManager];

    [self addSource:@"all" title:NSLocalizedString(@"All Messages", @"") path:nil group:nil];
    [self addSource:@"kernel" title:NSLocalizedString(@"Kernel", @"") path:nil group:nil];
    [self addSource:@"problems" title:NSLocalizedString(@"Errors and Faults", @"") path:nil group:nil];
    for (NSString *n in [[fm contentsOfDirectoryAtPath:@"/var/log" error:NULL] sortedArrayUsingSelector:@selector(compare:)]) {
        BOOL dir = NO;
        NSString *full = [@"/var/log" stringByAppendingPathComponent:n];

        if ([n hasSuffix:@".log"] && [fm fileExistsAtPath:full isDirectory:&dir] && !dir)
            [self addSource:full title:n path:full group:@"/var/log"];
    }
    [self addSource:@"crash" title:NSLocalizedString(@"Crash Reports", @"") path:nil group:NSLocalizedString(@"Reports", @"")];
}

- (void)append:(NSString *)text source:(NSString *)key proc:(NSString *)proc
{
    for (NSString *line in [text componentsSeparatedByString:@"\n"]) {
        NSString *msg = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        NSString *who = proc, *stamp = nil;
        NSRange colon;
        LogLine *l;

        if ([msg length] == 0)
            continue;
        // NSLog lines bring their own time and process: "2026-10-17 06:32:05.928 Logs[45] text"
        if ([msg length] > 24 && [msg characterAtIndex:4] == '-' && [msg characterAtIndex:10] == ' ' && [msg characterAtIndex:13] == ':') {
            NSRange sp = [msg rangeOfString:@" " options:0 range:NSMakeRange(24, [msg length] - 24)];

            stamp = [msg substringWithRange:NSMakeRange(11, 12)];
            if (sp.location != NSNotFound) {
                who = [msg substringWithRange:NSMakeRange(24, sp.location - 24)];
                msg = [msg substringFromIndex:sp.location + 1];
            }
        }
        // the kernel buffer starts each line with its uptime in brackets
        if ([msg hasPrefix:@"["] && [msg rangeOfString:@"]"].location != NSNotFound && [msg rangeOfString:@"]"].location < 20) {
            msg = [msg substringFromIndex:[msg rangeOfString:@"]"].location + 1];
            if ([msg hasPrefix:@":"])
                msg = [msg substringFromIndex:1];
            msg = [msg stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        }
        // "neuswc[102]: frame ..." names its process, keep that apart from the message
        colon = [msg rangeOfString:@": "];
        if (stamp == nil && colon.location != NSNotFound && colon.location < 40 && [msg rangeOfString:@" "].location >= colon.location) {
            who = [msg substringToIndex:colon.location];
            msg = [msg substringFromIndex:NSMaxRange(colon)];
        }
        l = [[[LogLine alloc] init] autorelease];
        l->time = [(stamp ?: nowStamp()) copy];
        l->proc = [who copy];
        l->msg = [msg copy];
        l->source = [key copy];
        l->level = levelOf(msg);
        l->added = [NSDate timeIntervalSinceReferenceDate];
        [_lines addObject:l];
    }
    if ([_lines count] > MAX_LINES)
        [_lines removeObjectsInRange:NSMakeRange(0, [_lines count] - MAX_LINES)];
}

// the kernel buffer has no positions, so only lines after the last one seen are new
- (void)readKernel
{
    char *buf = malloc(128 * 1024);
    int n = proc_kmsgbuf(buf, 128 * 1024);
    NSString *all, *fresh;
    NSRange r;

    if (n <= 0) {
        free(buf);
        return;
    }
    all = [[[NSString alloc] initWithBytes:buf length:strnlen(buf, n) encoding:NSUTF8StringEncoding] autorelease];
    free(buf);
    if (all == nil)
        return;
    fresh = all;
    if (_kernelSeen != nil && (r = [all rangeOfString:_kernelSeen options:NSBackwardsSearch]).location != NSNotFound)
        fresh = [all substringFromIndex:NSMaxRange(r)];
    else if (_kernelSeen == nil && [all length] > 16384)
        fresh = [all substringFromIndex:[all length] - 16384];
    [_kernelSeen release];
    _kernelSeen = [[all substringFromIndex:MAX(0, (NSInteger)[all length] - 160)] retain];
    [self append:fresh source:@"kernel" proc:@"kernel[0]"];
}

- (void)readFile:(LogSource *)s
{
    NSFileHandle *h = [NSFileHandle fileHandleForReadingAtPath:s->path];
    NSString *text, *proc;
    NSData *data;
    NSRange last;
    struct stat st;

    if (h == nil || stat([s->path fileSystemRepresentation], &st) != 0)
        return;
    // a file that got shorter was rotated or emptied, read it again from the start
    if (st.st_size < s->offset)
        s->offset = 0;
    if (st.st_size == s->offset)
        return;
    [h seekToFileOffset:s->offset];
    data = [h readDataOfLength:st.st_size - s->offset];
    s->offset += [data length];
    text = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
    if (text == nil)
        text = [[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding] autorelease];
    if (s->partial != nil)
        text = [s->partial stringByAppendingString:text];
    [s->partial release];
    s->partial = nil;
    // a line still being written waits for its newline
    last = [text rangeOfString:@"\n" options:NSBackwardsSearch];
    if (last.location != NSNotFound && NSMaxRange(last) < [text length]) {
        s->partial = [[text substringFromIndex:NSMaxRange(last)] retain];
        text = [text substringToIndex:last.location];
    }
    proc = [s->title stringByDeletingPathExtension];
    [self append:text source:s->key proc:proc];
}

- (void)readCrashes
{
    static NSMutableSet *seen;
    NSString *dir = @"/Library/Logs/DiagnosticReports";

    if (seen == nil)
        seen = [[NSMutableSet alloc] init];
    for (NSString *n in [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:NULL] sortedArrayUsingSelector:@selector(compare:)]) {
        if ([seen containsObject:n])
            continue;
        [seen addObject:n];
        [self append:[NSString stringWithFormat:@"ReportCrash: saved crash report %@", [dir stringByAppendingPathComponent:n]] source:@"crash" proc:@"ReportCrash"];
    }
}

- (void)poll:(NSTimer *)timer
{
    NSUInteger before = [_lines count];

    if (_paused && timer != nil)
        return;
    [self readKernel];
    for (LogSource *s in _sources) {
        if (s->path != nil)
            [self readFile:s];
    }
    [self readCrashes];
    if ([_lines count] != before || timer == nil)
        [self filterLines];
}

- (BOOL)matches:(LogLine *)l
{
    NSString *q = [_filter stringValue];

    if ([_selected isEqualToString:@"problems"]) {
        if (l->level == LevelInfo)
            return NO;
    } else if (![_selected isEqualToString:@"all"] && ![l->source isEqualToString:_selected]) {
        return NO;
    }
    if (_problemsOnly && l->level == LevelInfo)
        return NO;
    if ([q length] > 0 && [l->msg rangeOfString:q options:NSCaseInsensitiveSearch].location == NSNotFound &&
        [l->proc rangeOfString:q options:NSCaseInsensitiveSearch].location == NSNotFound)
        return NO;
    return YES;
}

- (void)filterLines
{
    [_shown removeAllObjects];
    for (LogLine *l in _lines) {
        if ([self matches:l])
            [_shown addObject:l];
    }
    if (_follow)
        _scroll = MAX(0, [_shown count] * LINE_H + 8 - NSHeight([self tailRect]));
    [self setNeedsDisplay:YES];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    [self filterLines];
}

- (NSRect)tailRect
{
    NSRect b = [self bounds];

    return NSMakeRect(SIDE_W + 1, PD_TOOLBAR_H + 1, NSWidth(b) - SIDE_W - 1, NSHeight(b) - PD_TOOLBAR_H - 1 - PD_STATUS_H);
}

- (NSUInteger)countFor:(LogSource *)s
{
    NSUInteger n = 0;

    for (LogLine *l in _lines) {
        if ([s->key isEqualToString:@"all"] || ([s->key isEqualToString:@"problems"] ? l->level != LevelInfo : [l->source isEqualToString:s->key]))
            n++;
    }
    return n;
}

- (void)drawSidebarIn:(NSRect)side
{
    NSDictionary *row = PDText(PDFont(@"Inter-Regular", 13), PDColor(0x1b1b1b, 0xe6e6e6));
    NSDictionary *num = PDText(PDFont(@"Inter-Regular", 11), PDColor(0x8a8a8a, 0x8a8a8a));
    NSString *group = nil;
    CGFloat y = NSMinY(side) + 8;

    PDDrawSidebar(side);
    for (LogSource *s in _sources) {
        NSString *n;
        NSRect r;

        if (s->group != nil && ![s->group isEqualToString:group]) {
            group = s->group;
            y += 10;
            PDDrawSectionHeading(group, NSMakePoint(18, y));
            y += 18;
        }
        r = NSMakeRect(10, y, SIDE_W - 20, 22);
        if ([s->key isEqualToString:_selected])
            PDFillRounded(r, 6, PDAlpha(PDColor(0x000000, 0xffffff), .1));
        [PDResourceImage(s->path != nil ? @"folder-small" : @"logs-small") drawInRect:NSMakeRect(18, y + 3, 16, 16) fromRect:NSZeroRect
                                                                             operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        [PDFitted(s->title, row, SIDE_W - 90) drawAtPoint:NSMakePoint(41, y + 2) withAttributes:row];
        n = [NSString stringWithFormat:@"%lu", (unsigned long)[self countFor:s]];
        [n drawAtPoint:NSMakePoint(SIDE_W - 18 - [n sizeWithAttributes:num].width, y + 4) withAttributes:num];
        y += 22;
    }
}

- (void)drawTail
{
    NSRect t = [self tailRect];
    NSFont *mono = PDMono(11.5);
    NSDictionary *tm = PDText(mono, PDColor(0xa3a3a3, 0x6e6e72));
    NSDictionary *pr = PDText(mono, PDColor(0x6b6158, 0xb8aca2));
    NSDictionary *ms = PDText(mono, PDColor(0x1b1b1b, 0xe0e0e0));
    NSDictionary *warn = PDText(mono, PDColor(0x8c5a40, 0xdba78c));
    NSDictionary *err = PDText(mono, PDColor(0xa8302a, 0xff9a92));
    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    CGFloat y = NSMinY(t) + 4 - _scroll, tmW = ceil([@"00:00:00.000" sizeWithAttributes:tm].width) + 14;

    [PDColor(0xffffff, 0x1f1f21) setFill];
    NSRectFill(t);
    [NSGraphicsContext saveGraphicsState];
    [NSBezierPath clipRect:t];
    for (LogLine *l in _shown) {
        NSRect row = NSMakeRect(NSMinX(t), y, NSWidth(t), LINE_H);
        CGFloat x = NSMinX(t) + 12;

        if (NSMaxY(row) >= NSMinY(t) && NSMinY(row) <= NSMaxY(t)) {
            // problems get a quiet tint over the row, new lines fade in from the accent
            if (l->level == LevelError)
                [PDAlpha(PDColor(0xc83c34, 0xff6e64), PDDark() ? .09 : .075) setFill];
            else if (l->level == LevelWarn)
                [PDAlpha(PDColor(0xb06e50, 0xd69678), PDDark() ? .08 : .07) setFill];
            else
                [[NSColor clearColor] setFill];
            NSRectFillUsingOperation(row, NSCompositeSourceOver);
            if (now - l->added < 1.2) {
                [PDAlpha([NSColor controlAccentColor], .12 * (1 - (now - l->added) / 1.2)) setFill];
                NSRectFillUsingOperation(row, NSCompositeSourceOver);
            }
            [l->time drawAtPoint:NSMakePoint(x, y + 2) withAttributes:tm];
            x += tmW;
            [PDFitted(l->proc, pr, 150) drawAtPoint:NSMakePoint(x, y + 2) withAttributes:pr];
            x += 160;
            [PDFitted(l->msg, l->level == LevelError ? err : l->level == LevelWarn ? warn : ms, NSMaxX(t) - x - 12)
                drawAtPoint:NSMakePoint(x, y + 2) withAttributes:l->level == LevelError ? err : l->level == LevelWarn ? warn : ms];
        }
        y += LINE_H;
    }
    [NSGraphicsContext restoreGraphicsState];
}

- (NSString *)sourceDescription
{
    if ([_selected isEqualToString:@"all"])
        return NSLocalizedString(@"kernel buffer and /var/log", @"");
    if ([_selected isEqualToString:@"kernel"])
        return @"dmesg";
    if ([_selected isEqualToString:@"problems"])
        return NSLocalizedString(@"every source", @"");
    if ([_selected isEqualToString:@"crash"])
        return @"/Library/Logs/DiagnosticReports";
    return _selected;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];
    NSRect find = NSMakeRect(NSWidth(b) - 180, 9, 170, 26);
    NSString *title = @"";

    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:8 yRadius:8] addClip];
    for (LogSource *s in _sources) {
        if ([s->key isEqualToString:_selected])
            title = s->title;
    }
    PDDrawToolbar(self, NSMakeRect(0, 0, NSWidth(b), PD_TOOLBAR_H));
    PDDrawTitle(NSLocalizedString(@"Logs", @""), title, NSMakePoint(PD_LIGHTS_W, PD_TOOLBAR_H / 2), 300, NO);
    [_pause->glyph release];
    _pause->glyph = [_paused ? @"play" : @"pause" copy];
    _pause->on = _paused;
    PDLayoutButtons(_buttons, NSMinX(find) - 8, PD_TOOLBAR_H, NO);
    PDFillRounded(find, 6, PDAlpha(PDColor(0x000000, 0xffffff), .06));
    PDDrawGlyph(@"search", NSMakeRect(NSMinX(find) + 8, NSMidY(find) - 6, 12, 12), PDColor(0x8a8a8a, 0x8a8a8a));
    [_filter setFrame:NSMakeRect(NSMinX(find) + 26, NSMinY(find) + 5, NSWidth(find) - 32, 17)];
    if ([[_filter stringValue] length] == 0 && [[self window] firstResponder] != [_filter currentEditor])
        [NSLocalizedString(@"Filter", @"") drawAtPoint:NSMakePoint(NSMinX(find) + 28, NSMinY(find) + 5) withAttributes:PDText(PDFont(@"Inter-Regular", 12), PDColor(0x9a9a9a, 0x8a8a8a))];

    [self drawSidebarIn:NSMakeRect(0, PD_TOOLBAR_H + 1, SIDE_W, NSHeight(b) - PD_TOOLBAR_H - 1)];
    [self drawTail];
    PDDrawStatus(NSMakeRect(SIDE_W + 1, NSHeight(b) - PD_STATUS_H, NSWidth(b) - SIDE_W - 1, PD_STATUS_H),
                 [NSString stringWithFormat:NSLocalizedString(@"%lu of %lu messages   %@", @""), (unsigned long)[_shown count], (unsigned long)[_lines count], [self sourceDescription]],
                 _paused ? NSLocalizedString(@"Paused", @"") : NSLocalizedString(@"Following new messages", @""));
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSString *group = nil;
    CGFloat y = PD_TOOLBAR_H + 1 + 8;

    if (PDClickButtons(_buttons, p))
        return;
    if (p.y < PD_TOOLBAR_H) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    if (p.x >= SIDE_W)
        return;
    for (LogSource *s in _sources) {
        if (s->group != nil && ![s->group isEqualToString:group]) {
            group = s->group;
            y += 28;
        }
        if (p.y >= y && p.y < y + 22) {
            _selected = s->key;
            _follow = YES;
            [self filterLines];
            return;
        }
        y += 22;
    }
}

- (void)scrollWheel:(NSEvent *)event
{
    CGFloat max = MAX(0, [_shown count] * LINE_H + 8 - NSHeight([self tailRect]));

    _scroll = MIN(MAX(_scroll - [event deltaY] * LINE_H * 3, 0), max);
    // scrolled back to the end it follows again
    _follow = _scroll >= max - 1;
    [self setNeedsDisplay:YES];
}

- (IBAction)togglePause:(id)sender
{
    _paused = !_paused;
    if (!_paused)
        [self poll:nil];
    [self setNeedsDisplay:YES];
}

- (IBAction)clear:(id)sender
{
    [_lines removeAllObjects];
    [self filterLines];
}

- (IBAction)toggleProblems:(id)sender
{
    _problemsOnly = !_problemsOnly;
    [self filterLines];
}

- (IBAction)find:(id)sender
{
    [[self window] makeFirstResponder:_filter];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
    if ([item action] == @selector(togglePause:))
        [item setTitle:_paused ? NSLocalizedString(@"Resume", @"") : NSLocalizedString(@"Pause", @"")];
    if ([item action] == @selector(toggleProblems:))
        [item setState:_problemsOnly ? NSOnState : NSOffState];
    return YES;
}

@end

@interface LogsController : NSObject {
    NSWindow *_window;
    LogsView *_view;
}
@end

@implementation LogsController

- (void)buildMenu
{
    NSMenu *windowMenu;
    NSMenu *main = PDStandardMenus(NSLocalizedString(@"Logs", @""), &windowMenu);
    NSMenu *edit = PDAddMenu(main, NSLocalizedString(@"Edit", @""));
    NSMenu *view = PDAddMenu(main, NSLocalizedString(@"View", @""));

    PDAddItem(edit, NSLocalizedString(@"Copy", @""), @selector(copy:), @"c", 0);
    PDAddItem(edit, NSLocalizedString(@"Find…", @""), @selector(find:), @"f", 0);
    PDAddItem(view, NSLocalizedString(@"Pause", @""), @selector(togglePause:), @"p", 0);
    PDAddItem(view, NSLocalizedString(@"Clear", @""), @selector(clear:), @"k", 0);
    [view addItem:[NSMenuItem separatorItem]];
    PDAddItem(view, NSLocalizedString(@"Errors and Warnings Only", @""), @selector(toggleProblems:), @"", 0);
    [PDAddItem(main, NSLocalizedString(@"Window", @""), NULL, @"", 0) setSubmenu:windowMenu];
    [NSApp setMainMenu:main];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self buildMenu];
    _window = [PDMakeWindow(NSMakeSize(860, 540), NSMakeSize(560, 320), -20, self) retain];
    _view = [[LogsView alloc] initWithFrame:[[_window contentView] bounds]];
    [_window setContentView:_view];
    [_window setTitle:NSLocalizedString(@"Logs", @"")];
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
    LogsController *controller = [[LogsController alloc] init];

    [app setDelegate:controller];
    [app run];
    [controller release];
    [pool release];
    return 0;
}
