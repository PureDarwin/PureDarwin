// akff2.m - drag and drop, clipboard, idle time and keyboard layout checks that need a running desktop
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <stdio.h>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); failures++; } else printf("ok   %s\n", #cond); } while (0)

@interface DragTarget : NSView {
@public
    int entered, updated, exited, performed;
    NSString *received;
    NSDragOperation offered;
}
@end

@implementation DragTarget

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info
{
    entered++;
    offered = [info draggingSourceOperationMask];
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info
{
    updated++;
    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)info
{
    exited++;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
{
    performed++;
    received = [[[info draggingPasteboard] stringForType:NSPasteboardTypeString] copy];
    return YES;
}

@end

@interface DragSource : NSView <NSDraggingSource> {
@public
    int began, ended;
    NSDragOperation endOperation;
}
@end

@implementation DragSource

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    return NSDragOperationCopy | NSDragOperationMove;
}

- (void)draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)point
{
    began++;
}

- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)point operation:(NSDragOperation)operation
{
    ended++;
    endOperation = operation;
}

@end

static NSEvent *MouseEvent(NSEventType type, NSWindow *window, NSPoint location)
{
    return [NSEvent mouseEventWithType:type location:location modifierFlags:0 timestamp:[[NSProcessInfo processInfo] systemUptime]
                          windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1];
}

// the app's event loop, which also services main-thread performs and timers
static void Spin(NSTimeInterval seconds)
{
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];

    while ([until timeIntervalSinceNow] > 0) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES];
        if (event != nil)
            [NSApp sendEvent:event];
    }
}

// a drag inside one window from a source view to a target view, driven by posted mouse events
static void TestDrag(void)
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 400, 200) styleMask:NSTitledWindowMask
                                                     backing:NSBackingStoreBuffered defer:NO];
    DragSource *source = [[DragSource alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)];
    DragTarget *target = [[DragTarget alloc] initWithFrame:NSMakeRect(200, 0, 200, 200)];
    NSImage *image = [NSImage imageWithSize:NSMakeSize(16, 16) flipped:NO drawingHandler:^BOOL(NSRect r) {
        [[NSColor blueColor] set];
        NSRectFill(r);
        return YES;
    }];
    NSDraggingItem *item = [[NSDraggingItem alloc] initWithPasteboardWriter:@"dragged text"];

    [[window contentView] addSubview:source];
    [[window contentView] addSubview:target];
    [target registerForDraggedTypes:@[ NSPasteboardTypeString ]];
    [window makeKeyAndOrderFront:nil];
    Spin(1.0);

    [item setDraggingFrame:NSMakeRect(50, 50, 16, 16) contents:image];
    NSEvent *down = MouseEvent(NSLeftMouseDown, window, NSMakePoint(58, 58));
    [source beginDraggingSessionWithItems:@[ item ] event:down source:source];
    [NSApp postEvent:MouseEvent(NSLeftMouseDragged, window, NSMakePoint(120, 60)) atStart:NO];
    [NSApp postEvent:MouseEvent(NSLeftMouseDragged, window, NSMakePoint(260, 70)) atStart:NO];
    [NSApp postEvent:MouseEvent(NSLeftMouseDragged, window, NSMakePoint(300, 80)) atStart:NO];
    [NSApp postEvent:MouseEvent(NSLeftMouseUp, window, NSMakePoint(300, 80)) atStart:NO];
    Spin(2.0);

    CHECK(source->began == 1);
    CHECK(target->entered == 1);
    CHECK(target->updated >= 1);
    CHECK(target->performed == 1);
    CHECK([target->received isEqualToString:@"dragged text"]);
    CHECK(target->offered == (NSDragOperationCopy | NSDragOperationMove));
    CHECK(source->ended == 1 && source->endOperation == NSDragOperationCopy);
    [window orderOut:nil];
}

// a minimal UCKeyTranslate: one key in one table with dead key states, enough to check the uchr data
static UniChar Translate(CFDataRef data, UInt16 key, UInt32 modifiers, UInt32 *deadState)
{
    const UInt8 *b = CFDataGetBytePtr(data);
    UInt32 modOff = *(const UInt32 *)(b + 20), idxOff = *(const UInt32 *)(b + 24);
    UInt32 recIdx = *(const UInt32 *)(b + 28), termOff = *(const UInt32 *)(b + 32);
    UInt32 m = (modifiers >> 8) & 0xff;
    UInt8 table = m < *(const UInt32 *)(b + modOff + 4) ? b[modOff + 8 + m] : 0;
    UInt32 tableOff = *(const UInt32 *)(b + idxOff + 8 + 4 * table);
    UInt16 output = *(const UInt16 *)(b + tableOff + 2 * key);

    if ((output & 0xc000) == 0x4000 && recIdx != 0) {
        const UInt16 *record = (const UInt16 *)(b + *(const UInt32 *)(b + recIdx + 4 + 4 * (output & 0x3fff)));
        if (*deadState != 0) {
            for (UInt16 i = 0; i < record[2]; i++) {
                if (record[4 + 2 * i] == *deadState) {
                    *deadState = 0;
                    return record[5 + 2 * i];
                }
            }
            UniChar terminator = ((const UInt16 *)(b + termOff + 4))[*deadState - 1];
            *deadState = record[1];
            return terminator;
        }
        *deadState = record[1];
        return record[0];
    }
    *deadState = 0;
    return output;
}

static void TestLayouts(void)
{
    const void *keys[] = { kTISPropertyInputSourceType };
    const void *values[] = { kTISTypeKeyboardLayout };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef list = TISCreateInputSourceList(query, false);
    TISInputSourceRef german = NULL, us = NULL, current;
    UInt32 dead = 0;

    CHECK(CFArrayGetCount(list) == 3);
    for (CFIndex i = 0; i < CFArrayGetCount(list); i++) {
        TISInputSourceRef source = (TISInputSourceRef)CFArrayGetValueAtIndex(list, i);
        CFStringRef sourceID = TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
        if (CFEqual(sourceID, CFSTR("com.apple.keylayout.German")))
            german = source;
        if (CFEqual(sourceID, CFSTR("com.apple.keylayout.US")))
            us = source;
    }
    CHECK(german != NULL && us != NULL);

    CFDataRef usData = TISGetInputSourceProperty(us, kTISPropertyUnicodeKeyLayoutData);
    CHECK(Translate(usData, 0, optionKey, &dead) == 0xe5);
    dead = 0;
    CHECK(Translate(usData, 14, optionKey, &dead) == 0xffff && dead == 2);
    CHECK(Translate(usData, 14, 0, &dead) == 0xe9 && dead == 0);
    CHECK(Translate(usData, 45, optionKey, &dead) == 0xffff && Translate(usData, 45, 0, &dead) == 0xf1);
    CHECK(Translate(usData, 14, optionKey, &dead) == 0xffff && Translate(usData, 49, 0, &dead) == 0xb4);

    CHECK(TISSelectInputSource(german) == noErr);
    current = TISCopyCurrentKeyboardLayoutInputSource();
    CHECK(CFEqual(TISGetInputSourceProperty(current, kTISPropertyInputSourceID), CFSTR("com.apple.keylayout.German")));
    CFDataRef deData = TISGetInputSourceProperty(current, kTISPropertyUnicodeKeyLayoutData);
    dead = 0;
    CHECK(Translate(deData, 16, 0, &dead) == 'z' && Translate(deData, 6, 0, &dead) == 'y');
    CHECK(Translate(deData, 37, optionKey, &dead) == '@');
    CHECK(Translate(deData, 10, 0, &dead) == 0xffff && Translate(deData, 0, 0, &dead) == 0xe2);
    CFRelease(current);
    CHECK(TISSelectInputSource(us) == noErr);
    CFRelease(list);
    CFRelease(query);
}

// the minimal image has no sync command, so output reaches the disk before the VM is stopped
static void FlushOutput(void)
{
    fflush(stdout);
    fsync(1);
    sync();
}

int main(int argc, char **argv)
{
    atexit(FlushOutput);
    setvbuf(stdout, NULL, _IOLBF, 0);
    @autoreleasepool {
        const char *mode = argc > 1 ? argv[1] : "all";

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];

        if (!strcmp(mode, "copy")) {
            NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(50, 50, 200, 100) styleMask:NSTitledWindowMask
                                                             backing:NSBackingStoreBuffered defer:NO];
            [window makeKeyAndOrderFront:nil];
            Spin(1.0);
            [[NSPasteboard generalPasteboard] clearContents];
            [[NSPasteboard generalPasteboard] setString:[NSString stringWithUTF8String:argc > 2 ? argv[2] : "hello"] forType:NSPasteboardTypeString];
            printf("copied\n");
            FlushOutput();
            Spin(argc > 3 ? atof(argv[3]) : 30);
            return 0;
        }
        if (!strcmp(mode, "paste")) {
            NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(300, 50, 200, 100) styleMask:NSTitledWindowMask
                                                             backing:NSBackingStoreBuffered defer:NO];
            [window makeKeyAndOrderFront:nil];
            Spin(2.0);
            NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
            printf("pasted [%s] types %s\n", [text UTF8String] ?: "(null)",
                   [[[[NSPasteboard generalPasteboard] types] componentsJoinedByString:@","] UTF8String]);
            CHECK(argc < 3 || [text isEqualToString:[NSString stringWithUTF8String:argv[2]]]);
        }
        if (!strcmp(mode, "idle")) {
            printf("mode idle: %.1f s since the last input\n", CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType));
            return 0;
        }
        if (!strcmp(mode, "all") || !strcmp(mode, "drag"))
            TestDrag();
        if (!strcmp(mode, "all") || !strcmp(mode, "layouts"))
            TestLayouts();
    }
    printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures != 0;
}
