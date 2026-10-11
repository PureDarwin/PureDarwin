// Apple's AAPLRenderer (MIT, unchanged) in an AppKit window on PureDarwin: NSApplication, an NSWindow and an
// MTKView as its content view, presenting into a Wayland subsurface of the window
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <stdio.h>

#import "AAPLRenderer.h"

@interface TriangleDelegate : NSObject <NSApplicationDelegate>
@end

@implementation TriangleDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)application {
  (void)application;
  return YES;
}
@end

static void on_exception(NSException *e) {
  fprintf(stderr, "triangle-appkit: uncaught %s: %s\n", [[e name] UTF8String], [[e reason] UTF8String]);
}

int main(int argc, char **argv) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSRect frame = NSMakeRect(100, 100, 800, 600);
  id<MTLDevice> device;
  NSWindow *window;
  MTKView *view;
  AAPLRenderer *renderer;

  (void)argc, (void)argv;
  NSSetUncaughtExceptionHandler(on_exception);
  [NSApplication sharedApplication];
  [NSApp setDelegate:[[TriangleDelegate alloc] init]];
  if (!(device = MTLCreateSystemDefaultDevice())) {
    fprintf(stderr, "triangle-appkit: no Metal device\n");
    return 1;
  }
  window = [[NSWindow alloc] initWithContentRect:frame
                                       styleMask:NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
  [window setTitle:@"Metal (ObjC, AppKit): AAPLRenderer triangle"];

  view = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height) device:device];
  renderer = [[AAPLRenderer alloc] initWithMetalKitView:view];
  [renderer mtkView:view drawableSizeWillChange:view.drawableSize];
  view.delegate = renderer;
  [window setContentView:view];
  [window makeKeyAndOrderFront:nil];

  [NSApp run];
  [pool release];
  return 0;
}
