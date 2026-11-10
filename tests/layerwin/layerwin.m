// a narrow titled window whose content is a layer-hosting view, the way gecko draws:
// sublayers with backgrounds and IOSurface contents changed inside CATransactions
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <IOSurface/IOSurface.h>

static IOSurfaceRef makeSurface(uint32_t bgra) {
    NSDictionary *props = @{ (id)kIOSurfaceWidth: @64, (id)kIOSurfaceHeight: @64,
                             (id)kIOSurfaceBytesPerElement: @4, (id)kIOSurfacePixelFormat: @((uint32_t)'BGRA') };
    IOSurfaceRef surface = IOSurfaceCreate((CFDictionaryRef)props);
    IOSurfaceLock(surface, 0, NULL);
    uint8_t *base = IOSurfaceGetBaseAddress(surface);
    size_t row = IOSurfaceGetBytesPerRow(surface);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            ((uint32_t *)(base + y * row))[x] = bgra;
    IOSurfaceUnlock(surface, 0, NULL);
    fprintf(stderr, "LAYERWIN surface %08x %zux%zu\n", IOSurfaceGetPixelFormat(surface), IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));
    return surface;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(300, 300, 120, 100)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
            backing:NSBackingStoreBuffered defer:NO];
        [window setTitle:@"Profile Missing"];

        NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 120, 100)];
        CALayer *root = [CALayer layer];
        [view setLayer:root];
        [view setWantsLayer:YES];
        [window setContentView:view];

        CALayer *background = [CALayer layer];
        background.frame = CGRectMake(0, 0, 120, 100);
        background.backgroundColor = CGColorCreateGenericRGB(0, 0, 1, 1);
        [root addSublayer:background];

        CALayer *tile = [CALayer layer];
        tile.frame = CGRectMake(28, 18, 64, 64);
        tile.cornerRadius = 8;
        tile.masksToBounds = YES;
        [root addSublayer:tile];

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        // red, then green after two seconds, so a stale frame shows
        __block int step = 0;
        [NSTimer scheduledTimerWithTimeInterval:2 repeats:YES block:^(NSTimer *timer) {
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            tile.contents = (__bridge_transfer id)makeSurface(step == 0 ? 0xffff0000 : 0xff00ff00);
            [CATransaction commit];
            fprintf(stderr, "LAYERWIN step %d %s\n", step, step == 0 ? "red" : "green");
            if (++step == 2)
                [timer invalidate];
        }];
        [NSApp run];
    }
    return 0;
}
