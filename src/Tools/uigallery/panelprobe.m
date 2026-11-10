// panelprobe.m - drives the NSOpenPanel and NSSavePanel API and prints what each call returns
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// usage: panelprobe DIR [interactive | keys], a timer ends each modal run with ok: or cancel:
// "interactive" waits for a person, "keys" posts key and mouse events and reports what the panels did

#import <AppKit/AppKit.h>
#import <dlfcn.h>
#import <objc/runtime.h>

static BOOL interactive;

static NSString *relative(NSURL *url, NSString *base)
{
    NSString *path = [[[url path] stringByStandardizingPath] stringByResolvingSymlinksInPath];

    if (path == nil)
        return @"(nil)";
    if ([path hasPrefix:base])
        return [@"DIR" stringByAppendingString:[path substringFromIndex:[base length]]];
    return path;
}

@interface Probe : NSObject
@property(copy) NSString *base;
@property BOOL rejectValidate;
@end

@implementation Probe

- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url
{
    return YES;
}

- (BOOL)panel:(id)sender validateURL:(NSURL *)url error:(NSError **)error
{
    printf("  delegate validateURL %s -> %s\n", [relative(url, _base) UTF8String], _rejectValidate ? "NO" : "YES");
    if (_rejectValidate && error != NULL)
        *error = [NSError errorWithDomain:@"probe" code:1 userInfo:@{ NSLocalizedDescriptionKey : @"rejected by probe" }];
    return !_rejectValidate;
}

- (void)panel:(id)sender didChangeToDirectoryURL:(NSURL *)url
{
    printf("  delegate didChangeToDirectoryURL %s\n", [relative(url, _base) UTF8String]);
}

- (NSString *)panel:(id)sender userEnteredFilename:(NSString *)filename confirmed:(BOOL)okFlag
{
    printf("  delegate userEnteredFilename %s confirmed %d\n", [filename UTF8String], okFlag);
    return filename;
}

- (void)panelSelectionDidChange:(id)sender
{
}

@end


static void dumpViews(NSView *v, int depth)
{
    NSRect f = [v frame];

    if (depth > 16)
        return;
    fprintf(stderr, "%*s%s %.0f,%.0f %.0fx%.0f\n", depth * 2, "", class_getName([v class]), f.origin.x, f.origin.y, f.size.width, f.size.height);
    for (NSView *s in [v subviews])
        dumpViews(s, depth + 1);
}

static void endLater(NSSavePanel *panel, BOOL ok, NSTimeInterval delay)
{
    if (interactive)
        return;
    NSTimer *timer = [NSTimer timerWithTimeInterval:delay repeats:NO block:^(NSTimer *t) {
        fprintf(stderr, "  timer %s visible=%d frame=%s\n", ok ? "ok" : "cancel", [panel isVisible], [NSStringFromRect([panel frame]) UTF8String]);
        if (getenv("PROBE_VIEWS"))
            dumpViews([[panel contentView] superview], 0);
        if (getenv("PROBE_SHOT")) {
            NSView *frame = [[panel contentView] superview];
            NSBitmapImageRep *rep = [frame bitmapImageRepForCachingDisplayInRect:[frame bounds]];
            static int n;

            [frame cacheDisplayInRect:[frame bounds] toBitmapImageRep:rep];
            [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[NSString stringWithFormat:@"%s/panel-%d.png", getenv("PROBE_SHOT"), n++] atomically:NO];
        }
        if (ok)
            [panel ok:nil];
        else
            [panel cancel:nil];
    }];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
}

static void dumpSave(NSString *label, NSSavePanel *p, NSString *base)
{
    printf("%s\n", [label UTF8String]);
    printf("  title=%s message=%s prompt=%s nameFieldLabel=%s\n", [[p title] UTF8String], [[p message] UTF8String],
        [[p prompt] UTF8String], [[p nameFieldLabel] UTF8String]);
    printf("  nameFieldStringValue=%s\n", [[p nameFieldStringValue] UTF8String]);
    printf("  allowedFileTypes=%s allowedContentTypes=%lu\n", [[[p allowedFileTypes] componentsJoinedByString:@","] UTF8String],
        (unsigned long)[[p allowedContentTypes] count]);
    printf("  allowsOtherFileTypes=%d canCreateDirectories=%d showsHiddenFiles=%d extensionHidden=%d canSelectHiddenExtension=%d treatsFilePackagesAsDirectories=%d\n",
        [p allowsOtherFileTypes], [p canCreateDirectories], [p showsHiddenFiles], [p isExtensionHidden],
        [p canSelectHiddenExtension], [p treatsFilePackagesAsDirectories]);
    if ([p isKindOfClass:[NSOpenPanel class]]) {
        NSOpenPanel *o = (NSOpenPanel *)p;

        printf("  canChooseFiles=%d canChooseDirectories=%d allowsMultipleSelection=%d resolvesAliases=%d canDownloadUbiquitousContents=%d\n",
            [o canChooseFiles], [o canChooseDirectories], [o allowsMultipleSelection], [o resolvesAliases],
            [o canDownloadUbiquitousContents]);
    }
}

static void result(NSString *label, NSInteger r, NSSavePanel *p, NSString *base)
{
    printf("%s -> %ld URL=%s\n", [label UTF8String], (long)r, [relative([p URL], base) UTF8String]);
    if ([p isKindOfClass:[NSOpenPanel class]]) {
        for (NSURL *u in [(NSOpenPanel *)p URLs])
            printf("  URLs[] %s\n", [relative(u, base) UTF8String]);
    }
}

// with PROBE_SNAP set, the key window is drawn to a PNG every second while a person drives the panels
static void snapLater(void)
{
    const char *dir = getenv("PROBE_SNAP");

    if (dir == NULL)
        return;
    NSTimer *timer = [NSTimer timerWithTimeInterval:1.0 repeats:YES block:^(NSTimer *t) {
        NSWindow *key = [NSApp keyWindow];
        NSView *frame = [[key contentView] superview];
        NSPoint mouse = [NSEvent mouseLocation];

        if (frame == nil)
            return;
        NSBitmapImageRep *rep = [frame bitmapImageRepForCachingDisplayInRect:[frame bounds]];

        [frame cacheDisplayInRect:[frame bounds] toBitmapImageRep:rep];
        [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[NSString stringWithFormat:@"%s/key.png", dir] atomically:YES];
        [[NSString stringWithFormat:@"%@ %@ mouse %.0f,%.0f\n", [key className], NSStringFromRect([key frame]), mouse.x, mouse.y]
            writeToFile:[NSString stringWithFormat:@"%s/key.txt", dir] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    }];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSEventTrackingRunLoopMode];
}

static NSString *snapDir;
static int snapCount;

static void snapWindow(NSWindow *w, NSString *label)
{
    NSView *frame = [[w contentView] superview];

    if (snapDir == nil || frame == nil)
        return;
    NSBitmapImageRep *rep = [frame bitmapImageRepForCachingDisplayInRect:[frame bounds]];

    [frame cacheDisplayInRect:[frame bounds] toBitmapImageRep:rep];
    [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[NSString stringWithFormat:@"%@/step-%02d-%@.png", snapDir, snapCount++, label] atomically:YES];
}

static NSView *findView(NSView *v, BOOL (^match)(NSView *))
{
    if (match(v))
        return v;
    for (NSView *s in [v subviews]) {
        NSView *found = findView(s, match);

        if (found != nil)
            return found;
    }
    return nil;
}

static NSWindow *keyOrPanel(NSWindow *panel)
{
    NSWindow *key = [NSApp keyWindow];

    if ([panel attachedSheet] != nil)
        return [panel attachedSheet];
    return key ?: panel;
}

static void postKey(NSWindow *w, NSString *chars, unsigned short code, NSUInteger mods)
{
    for (int i = 0; i < 2; i++) {
        NSEvent *e = [NSEvent keyEventWithType:i ? NSEventTypeKeyUp : NSEventTypeKeyDown location:NSZeroPoint modifierFlags:mods
            timestamp:[[NSProcessInfo processInfo] systemUptime] windowNumber:[w windowNumber] context:nil
            characters:chars charactersIgnoringModifiers:chars isARepeat:NO keyCode:code];

        [NSApp postEvent:e atStart:NO];
    }
}

static void typeText(NSWindow *w, NSString *text)
{
    for (NSUInteger i = 0; i < [text length]; i++)
        postKey(w, [text substringWithRange:NSMakeRange(i, 1)], 0, 0);
}

static void postClick(NSWindow *w, NSPoint p, int count)
{
    for (int n = 1; n <= count; n++) {
        for (int i = 0; i < 2; i++) {
            NSEvent *e = [NSEvent mouseEventWithType:i ? NSEventTypeLeftMouseUp : NSEventTypeLeftMouseDown location:p modifierFlags:0
                timestamp:[[NSProcessInfo processInfo] systemUptime] windowNumber:[w windowNumber] context:nil
                eventNumber:0 clickCount:n pressure:i ? 0 : 1];

            [NSApp postEvent:e atStart:NO];
        }
    }
}

static void clickView(NSWindow *w, NSView *v, int count)
{
    NSRect r = [v convertRect:[v bounds] toView:nil];

    postClick(w, NSMakePoint(NSMidX(r), NSMidY(r)), count);
}

static NSButton *buttonTitled(NSWindow *w, NSString *title)
{
    return (NSButton *)findView([[w contentView] superview], ^BOOL(NSView *v) {
        return [v isKindOfClass:[NSButton class]] && [[(NSButton *)v title] isEqualToString:title] && ![v isHiddenOrHasHiddenAncestor];
    });
}

static NSTableView *fileList(NSWindow *w)
{
    return (NSTableView *)findView([w contentView], ^BOOL(NSView *v) {
        return [v isKindOfClass:[NSTableView class]] && [(NSTableView *)v headerView] != nil;
    });
}

static void clickRow(NSWindow *w, NSString *name, int count)
{
    NSTableView *table = fileList(w);
    NSInteger column = [table columnWithIdentifier:@"name"];

    for (NSInteger row = 0; row < [table numberOfRows]; row++) {
        if ([[[table dataSource] tableView:table objectValueForTableColumn:[table tableColumns][column] row:row] isEqual:name]) {
            NSRect r = [table convertRect:[table frameOfCellAtColumn:column row:row] toView:nil];

            postClick(w, NSMakePoint(NSMidX(r), NSMidY(r)), count);
            return;
        }
    }
    printf("  no row %s\n", [name UTF8String]);
}

static NSString *rows(NSWindow *w)
{
    NSTableView *table = fileList(w);
    NSInteger column = [table columnWithIdentifier:@"name"];
    NSMutableArray *names = [NSMutableArray array];

    for (NSInteger row = 0; row < [table numberOfRows]; row++) {
        NSString *name = [[table dataSource] tableView:table objectValueForTableColumn:[table tableColumns][column] row:row];

        [names addObject:[table isRowSelected:row] ? [NSString stringWithFormat:@"[%@]", name] : name];
    }
    return [names componentsJoinedByString:@" "];
}

// runs one step a second inside whatever modal loop is current
static void steps(NSArray *list)
{
    __block NSUInteger index = 0;
    NSArray *copy = [list copy];
    NSTimer *timer = [NSTimer timerWithTimeInterval:0.8 repeats:YES block:^(NSTimer *t) {
        if (index >= [copy count]) {
            [t invalidate];
            return;
        }
        void (^step)(void) = copy[index++];

        step();
    }];

    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
}

static void keyScenarios(NSString *base, Probe *probe)
{
    NSString *(^rel)(NSURL *) = ^NSString *(NSURL *u) { return relative(u, base); };
    NSInteger r;

    // typing a bare name in the compact panel adds the allowed extension, Return confirms
    NSSavePanel *save = [NSSavePanel savePanel];
    [save setDirectoryURL:[NSURL fileURLWithPath:base]];
    [save setAllowedFileTypes:@[ @"txt" ]];
    [save setDelegate:probe];
    steps(@[
        ^{ postKey(save, @"a", 0, NSEventModifierFlagCommand); typeText(save, @"hello"); },
        ^{ snapWindow(save, @"save-typed"); postKey(save, @"\r", 36, 0); },
    ]);
    r = [save runModal];
    printf("keys: save typed hello, Return -> %ld URL=%s\n", (long)r, [rel([save URL]) UTF8String]);

    // an existing name asks before replacing, Return takes the default Cancel, then Replace is clicked
    NSSavePanel *over = [NSSavePanel savePanel];
    [over setDirectoryURL:[NSURL fileURLWithPath:base]];
    [over setNameFieldStringValue:@"existing.txt"];
    steps(@[
        ^{ postKey(over, @"\r", 36, 0); },
        ^{ NSWindow *s = [over attachedSheet]; NSTextField *f = (NSTextField *)findView([s contentView], ^BOOL(NSView *v) { return [v isKindOfClass:[NSTextField class]] && [[(NSTextField *)v stringValue] rangeOfString:@"already exists"].location != NSNotFound; }); printf("  replace alert up=%d text=%s\n", s != nil, [[f stringValue] UTF8String]); snapWindow(s, @"replace-alert"); postKey(s, @"\r", 36, 0); },
        ^{ printf("  after Return: panel visible=%d sheet=%d\n", [over isVisible], [over attachedSheet] != nil); postKey(over, @"\r", 36, 0); },
        ^{ NSWindow *s = [over attachedSheet]; clickView(s, buttonTitled(s, @"Replace"), 1); },
    ]);
    r = [over runModal];
    printf("keys: save over existing, Replace -> %ld URL=%s\n", (long)r, [rel([over URL]) UTF8String]);

    // type-select, Return opens a folder, Cmd-[ goes back, a double-click confirms a file
    NSOpenPanel *open = [NSOpenPanel openPanel];
    [open setDirectoryURL:[NSURL fileURLWithPath:base]];
    [open setDelegate:probe];
    steps(@[
        ^{ printf("  rows: %s\n", [rows(open) UTF8String]); typeText(open, @"su"); },
        ^{ printf("  after typing su: %s\n", [rows(open) UTF8String]); postKey(open, @"\r", 36, 0); },
        ^{ printf("  after Return: dir=%s rows: %s\n", [rel([open directoryURL]) UTF8String], [rows(open) UTF8String]); postKey(open, @"[", 33, NSEventModifierFlagCommand); },
        ^{ printf("  after Cmd-[: dir=%s\n", [rel([open directoryURL]) UTF8String]); postKey(open, @".", 47, NSEventModifierFlagCommand | NSEventModifierFlagShift); },
        ^{ printf("  after Cmd-Shift-.: %s\n", [rows(open) UTF8String]); postKey(open, @".", 47, NSEventModifierFlagCommand | NSEventModifierFlagShift); },
        ^{ NSTableView *t = fileList(open); NSTableColumn *c = [t tableColumnWithIdentifier:@"size"]; NSRect h = [[t headerView] convertRect:[[t headerView] headerRectOfColumn:[[t tableColumns] indexOfObject:c]] toView:nil]; postClick(open, NSMakePoint(NSMidX(h), NSMidY(h)), 1); },
        ^{ printf("  after Size header click: %s\n", [rows(open) UTF8String]); snapWindow(open, @"open-sorted"); clickRow(open, @"existing.txt", 2); },
    ]);
    r = [open runModal];
    printf("keys: open double-click existing.txt -> %ld URL=%s\n", (long)r, [rel([open URL]) UTF8String]);

    // Cmd-Shift-G asks for a path, Escape cancels the panel
    NSOpenPanel *go = [NSOpenPanel openPanel];
    [go setDirectoryURL:[NSURL fileURLWithPath:base]];
    steps(@[
        ^{ postKey(go, @"g", 5, NSEventModifierFlagCommand | NSEventModifierFlagShift); },
        ^{ NSWindow *k = [NSApp keyWindow]; snapWindow(k, @"goto"); postKey(k, @"a", 0, NSEventModifierFlagCommand); typeText(k, [base stringByAppendingPathComponent:@"sub"]); },
        ^{ postKey([NSApp keyWindow], @"\r", 36, 0); },
        ^{ printf("  after Go: dir=%s\n", [rel([go directoryURL]) UTF8String]); postKey(go, @"\e", 53, 0); },
    ]);
    r = [go runModal];
    printf("keys: open go to folder, Escape -> %ld\n", (long)r);

    // the expanded save panel makes a folder and saves into it
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSNavPanelExpandedStateForSaveMode"];
    NSSavePanel *folder = [NSSavePanel savePanel];
    [folder setDirectoryURL:[NSURL fileURLWithPath:base]];
    [folder setNameFieldStringValue:@"inside.txt"];
    steps(@[
        ^{ snapWindow(folder, @"save-expanded"); clickView(folder, buttonTitled(folder, @"New Folder"), 1); },
        ^{ NSWindow *k = [NSApp keyWindow]; snapWindow(k, @"new-folder"); postKey(k, @"a", 0, NSEventModifierFlagCommand); typeText(k, @"made"); },
        ^{ postKey([NSApp keyWindow], @"\r", 36, 0); },
        ^{ printf("  after New Folder: dir=%s\n", [rel([folder directoryURL]) UTF8String]); clickView(folder, buttonTitled(folder, @"Save"), 1); },
    ]);
    r = [folder runModal];
    printf("keys: save into new folder -> %ld URL=%s\n", (long)r, [rel([folder URL]) UTF8String]);
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"NSNavPanelExpandedStateForSaveMode"];

    // multiple selection with Cmd-click is not posted here, a range comes from Shift-Down
    NSOpenPanel *multi = [NSOpenPanel openPanel];
    [multi setDirectoryURL:[NSURL fileURLWithPath:base]];
    [multi setAllowsMultipleSelection:YES];
    steps(@[
        ^{ clickRow(multi, @"existing.txt", 1); },
        ^{ postKey(multi, [NSString stringWithFormat:@"%C", (unichar)NSDownArrowFunctionKey], 125, NSEventModifierFlagShift); },
        ^{ printf("  selected: %s\n", [rows(multi) UTF8String]); postKey(multi, @"\r", 36, 0); },
    ]);
    r = [multi runModal];
    printf("keys: open two files -> %ld\n", (long)r);
    for (NSURL *u in [multi URLs])
        printf("  URLs[] %s\n", [rel(u) UTF8String]);
}

static void uncaught(NSException *e)
{
    printf("uncaught %s: %s\n%s\n", [[e name] UTF8String], [[e reason] UTF8String], [[[e callStackSymbols] description] UTF8String]);
}

int main(int argc, char **argv)
{
    NSSetUncaughtExceptionHandler(uncaught);
    @autoreleasepool {
        setvbuf(stdout, NULL, _IOLBF, 0);
        NSString *base = argc > 1 ? [[NSString stringWithUTF8String:argv[1]] stringByStandardizingPath] : @"/tmp/panelprobe";
        NSFileManager *fm = [NSFileManager defaultManager];
        Probe *probe = [Probe new];

        interactive = argc > 2 && strcmp(argv[2], "interactive") == 0;
        BOOL keys = argc > 2 && strcmp(argv[2], "keys") == 0;
        [probe setBase:base];
        [fm removeItemAtPath:base error:NULL];
        [fm createDirectoryAtPath:[base stringByAppendingPathComponent:@"sub"] withIntermediateDirectories:YES attributes:nil error:NULL];
        base = [base stringByResolvingSymlinksInPath];
        [probe setBase:base];
        [@"x" writeToFile:[base stringByAppendingPathComponent:@"existing.txt"] atomically:NO encoding:NSUTF8StringEncoding error:NULL];
        [@"x" writeToFile:[base stringByAppendingPathComponent:@"image.png"] atomically:NO encoding:NSUTF8StringEncoding error:NULL];

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
        snapLater();

        if (keys) {
            snapDir = getenv("PROBE_SNAP") ? [NSString stringWithUTF8String:getenv("PROBE_SNAP")] : nil;
            keyScenarios(base, probe);
            return 0;
        }

        NSSavePanel *save = [NSSavePanel savePanel];
        dumpSave(@"save defaults", save, base);
        NSOpenPanel *open = [NSOpenPanel openPanel];
        dumpSave(@"open defaults", open, base);

        [save setTitle:@"Probe Save"];
        [save setMessage:@"Pick a name"];
        [save setPrompt:@"Store"];
        [save setNameFieldLabel:@"File:"];
        [save setNameFieldStringValue:@"new.txt"];
        dlopen("/System/Library/Frameworks/UniformTypeIdentifiers.framework/UniformTypeIdentifiers", RTLD_LAZY);
        Class ut = NSClassFromString(@"UTType");
        [save setAllowedContentTypes:@[ [ut typeWithIdentifier:@"public.plain-text"], [ut typeWithIdentifier:@"public.png"] ]];
        [save setAllowsOtherFileTypes:YES];
        [save setCanCreateDirectories:NO];
        [save setShowsHiddenFiles:YES];
        [save setExtensionHidden:NO];
        [save setTreatsFilePackagesAsDirectories:YES];
        [save setDirectoryURL:[NSURL fileURLWithPath:base]];
        dumpSave(@"save after setters", save, base);
        printf("  directoryURL=%s\n", [relative([save directoryURL], base) UTF8String]);

        [save setDelegate:probe];
        endLater(save, YES, 1.5);
        NSInteger r = [save runModal];
        result(@"save runModal ok", r, save, base);
        printf("  nameFieldStringValue=%s\n", [[save nameFieldStringValue] UTF8String]);

        NSSavePanel *save2 = [NSSavePanel savePanel];
        [save2 setDirectoryURL:[NSURL fileURLWithPath:base]];
        [save2 setNameFieldStringValue:@"plain"];
        [save2 setAllowedFileTypes:@[ @"txt" ]];
        endLater(save2, YES, 1.5);
        r = [save2 runModal];
        result(@"save allowedFileTypes txt, name plain", r, save2, base);

        NSSavePanel *save3 = [NSSavePanel savePanel];
        [save3 setDirectoryURL:[NSURL fileURLWithPath:base]];
        [save3 setNameFieldStringValue:@"x.txt"];
        endLater(save3, NO, 1.5);
        r = [save3 runModal];
        result(@"save cancel", r, save3, base);

        NSSavePanel *save4 = [NSSavePanel savePanel];
        [save4 setDirectoryURL:[NSURL fileURLWithPath:base]];
        [save4 setNameFieldStringValue:@"rejected.txt"];
        [save4 setDelegate:probe];
        [probe setRejectValidate:YES];
        endLater(save4, YES, 1.5);
        endLater(save4, NO, 4.0);
        r = [save4 runModal];
        [probe setRejectValidate:NO];
        result(@"save validate rejects then cancel", r, save4, base);

        [open setDirectoryURL:[NSURL fileURLWithPath:base]];
        [open setCanChooseDirectories:YES];
        [open setCanChooseFiles:NO];
        [open setAllowsMultipleSelection:YES];
        [open setResolvesAliases:NO];
        [open setCanDownloadUbiquitousContents:YES];
        [open setDelegate:probe];
        dumpSave(@"open after setters", open, base);
        endLater(open, YES, 1.5);
        r = [open runModal];
        result(@"open directories runModal ok", r, open, base);

        NSOpenPanel *typed = [NSOpenPanel openPanel];
        [typed setDirectoryURL:[NSURL fileURLWithPath:base]];
        [typed setAllowedFileTypes:@[ @"txt", @"png" ]];
        [typed setAllowsMultipleSelection:YES];
        endLater(typed, YES, 1.5);
        endLater(typed, NO, 3.0);
        r = [typed runModal];
        result(@"open two types, ok with nothing selected then cancel", r, typed, base);

        NSWindow *doc = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 600, 400)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
        [doc setTitle:@"panelprobe document"];
        [doc makeKeyAndOrderFront:nil];

        __block BOOL done = NO;
        NSOpenPanel *sheet = [NSOpenPanel openPanel];
        [sheet setDirectoryURL:[NSURL fileURLWithPath:base]];
        endLater(sheet, NO, 1.5);
        [sheet beginSheetModalForWindow:doc completionHandler:^(NSModalResponse response) {
            printf("open sheet completion -> %ld attachedSheet=%d\n", (long)response, [doc attachedSheet] != nil);
            done = YES;
        }];
        printf("open sheet began, attachedSheet=%d isSheet=%d\n", [doc attachedSheet] == sheet, [sheet isSheet]);
        while (!done)
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

        done = NO;
        NSSavePanel *later = [NSSavePanel savePanel];
        [later setDirectoryURL:[NSURL fileURLWithPath:[base stringByAppendingPathComponent:@"sub"]]];
        [later setNameFieldStringValue:@"later.txt"];
        endLater(later, YES, 1.5);
        [later beginWithCompletionHandler:^(NSModalResponse response) {
            result(@"save beginWithCompletionHandler", response, later, base);
            done = YES;
        }];
        printf("save beginWithCompletionHandler returned\n");
        while (!done)
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

        printf("NSNavLastRootDirectory=%s\n", [[[NSUserDefaults standardUserDefaults] stringForKey:@"NSNavLastRootDirectory"] UTF8String]);
        NSSavePanel *fresh = [NSSavePanel savePanel];
        printf("fresh panel directoryURL=%s\n", [relative([fresh directoryURL], base) UTF8String]);
        printf("NSModalResponseOK=%ld NSModalResponseCancel=%ld NSFileHandlingPanelOKButton=%ld\n",
            (long)NSModalResponseOK, (long)NSModalResponseCancel, (long)NSFileHandlingPanelOKButton);
    }
    return 0;
}
