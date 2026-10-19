// akff.m - checks the AppKit and Carbon API Firefox 158 calls
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <stdio.h>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); failures++; } else printf("ok   %s\n", #cond); } while (0)

// a bare UCKeyTranslate over the uchr data, enough to read one key in one table
static UniChar KeyFromLayout(CFDataRef data, UInt16 key, UInt32 modifiers)
{
    const UInt8 *b = CFDataGetBytePtr(data);
    UInt32 modOff = *(const UInt32 *)(b + 20), idxOff = *(const UInt32 *)(b + 24);
    UInt32 count = *(const UInt32 *)(b + modOff + 4);
    UInt32 m = (modifiers >> 8) & 0xff;
    UInt8 table = m < count ? b[modOff + 8 + m] : *(const UInt16 *)(b + modOff + 2);
    UInt32 tableOff = *(const UInt32 *)(b + idxOff + 8 + 4 * table);

    return *(const UInt16 *)(b + tableOff + 2 * key);
}

int main(void)
{
    @autoreleasepool {
        [NSApplication sharedApplication];

        TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
        CHECK(source != NULL);
        CHECK(CFEqual(TISGetInputSourceProperty(source, kTISPropertyInputSourceID), CFSTR("com.apple.keylayout.US")));
        CFDataRef layout = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData);
        CHECK(layout != NULL && *(const UInt16 *)CFDataGetBytePtr(layout) == 0x1002);
        CHECK(KeyFromLayout(layout, 0, 0) == 'a');
        CHECK(KeyFromLayout(layout, 0, shiftKey) == 'A');
        CHECK(KeyFromLayout(layout, 18, shiftKey) == '!');
        CHECK(KeyFromLayout(layout, 0, controlKey) == 1);
        const void *keys[] = { kTISPropertyInputSourceType };
        const void *layoutType[] = { kTISTypeKeyboardLayout }, *modeType[] = { kTISTypeKeyboardInputMode };
        CFDictionaryRef query = CFDictionaryCreate(NULL, keys, layoutType, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFArrayRef list = TISCreateInputSourceList(query, false);
        CHECK(CFArrayGetCount(list) == 1);
        CFRelease(list);
        CFRelease(query);
        query = CFDictionaryCreate(NULL, keys, modeType, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        list = TISCreateInputSourceList(query, false);
        CFRelease(query);
        CHECK(CFArrayGetCount(list) == 0);
        CFRelease(list);
        CFRelease(source);
        CHECK(TISCopyInputMethodKeyboardLayoutOverride() == NULL);
        CHECK(KBGetLayoutType(LMGetKbdType()) == kKeyboardANSI);

        UInt32 value = 7, out = 0, size = 0;
        TSMDocumentID doc = TSMGetActiveDocument();
        CHECK(TSMSetDocumentProperty(doc, 'test', sizeof(value), &value) == noErr);
        CHECK(TSMGetDocumentProperty(doc, 'test', sizeof(out), &size, &out) == noErr && out == 7 && size == 4);
        CHECK(TSMRemoveDocumentProperty(doc, 'test') == noErr);
        CHECK(TSMRemoveDocumentProperty(doc, 'test') == tsmDocPropertyNotFoundErr);
        EnableSecureEventInput();
        CHECK(IsSecureEventInputEnabled());
        DisableSecureEventInput();
        CHECK(!IsSecureEventInputEnabled());
        SInt32 metric = 0;
        CHECK(GetThemeMetric(kThemeMetricCheckBoxWidth, &metric) == noErr && metric == 14);
        GetCurrentEventKeyModifiers();

        NSRunningApplication *me = [NSRunningApplication currentApplication];
        CHECK([me processIdentifier] == getpid());
        CHECK([[NSWorkspace sharedWorkspace] runningApplications] != nil);
        CHECK([NSRunningApplication runningApplicationWithProcessIdentifier:1] != nil);

        @try {
        NSPasteboard *pb = [NSPasteboard pasteboardWithName:@"akff-test"];
        [pb clearContents];
        CHECK([pb writeObjects:@[ @"hello" ]]);
        NSArray *read = [pb readObjectsForClasses:@[ [NSString class] ] options:nil];
        CHECK([read count] == 1 && [[read objectAtIndex:0] isEqualToString:@"hello"]);
        CHECK([[pb pasteboardItems] count] == 1);

        NSColor *p3 = [[NSColor colorWithDisplayP3Red:1 green:0 blue:0 alpha:1] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
        CHECK([p3 redComponent] > 0.99 && [p3 greenComponent] < 0.01);
        CHECK([NSColorSpace sRGBColorSpace] != nil);
        CHECK([NSColor systemPurpleColor] != nil);

        NSImage *image = [NSImage imageWithSize:NSMakeSize(4, 4) flipped:NO drawingHandler:^BOOL(NSRect r) {
            [[NSColor redColor] set];
            NSRectFill(r);
            return YES;
        }];
        NSRect rect = NSZeroRect;
        CGImageRef cg = [image CGImageForProposedRect:&rect context:nil hints:nil];
        CHECK(cg != NULL && CGImageGetWidth(cg) == 4);

        NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
        [view setAccessibilityLabel:@"lbl"];
        CHECK([[view accessibilityLabel] isEqualToString:@"lbl"]);

        NSSavePanel *panel = [NSSavePanel savePanel];
        [panel setDirectoryURL:[NSURL fileURLWithPath:@"/tmp"]];
        CHECK([[[panel directoryURL] path] isEqualToString:@"/tmp"]);
        [panel setNameFieldStringValue:@"a.html"];
        CHECK([[panel nameFieldStringValue] isEqualToString:@"a.html"]);

        NSEvent *key = [NSEvent keyEventWithType:NSKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0
                                         context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
        CGEventRef cgEvent = [key CGEvent];
        CHECK(cgEvent != NULL);
        CHECK([NSCursor columnResizeCursor] != nil && [NSCursor busyButClickableCursor] != nil);
        CHECK([NSScroller preferredScrollerStyle] == NSScrollerStyleLegacy);
        CHECK([[NSApp class] instancesRespondToSelector:@selector(_isRunningModal)] && ![NSApp _isRunningModal]);
        [[[NSMenu alloc] initWithTitle:@"m"] cancelTracking];
        // windows and controls need a compositor
        if (access("/var/run/user/0/wayland-0", F_OK) == 0) {
            NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 100) styleMask:NSTitledWindowMask
                                                             backing:NSBackingStoreBuffered defer:YES];
            CHECK(NSEqualSizes([window contentLayoutRect].size, NSMakeSize(200, 100)));
            [window setAnimationBehavior:NSWindowAnimationBehaviorNone];
            CHECK([window animationBehavior] == NSWindowAnimationBehaviorNone);
            NSTextField *label = [NSTextField labelWithString:@"x"];
            CHECK(![label isEditable]);
            NSButton *button = [NSButton buttonWithTitle:@"OK" target:nil action:NULL];
            CHECK([[button title] isEqualToString:@"OK"]);

        } else {
            printf("skip window and control checks, no display\n");
        }
        } @catch (NSException *e) {
            printf("FAIL exception %s: %s\n", [[e name] UTF8String], [[e reason] UTF8String]);
            failures++;
        }
    }
    printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures != 0;
}
