// Apple's "Using a Render Pipeline to Render Primitives" renderer (AAPLRenderer, MIT, unchanged) in an X11
// window on PureDarwin: Metal is darling-metal over indium-pd, MTKView is PD's GLFW one
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <stdio.h>

#import "AAPLRenderer.h"

static void on_exception(NSException *e) {
  fprintf(stderr, "triangle: uncaught %s: %s\n", [[e name] UTF8String], [[e reason] UTF8String]);
}

int main(int argc, char **argv) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  MTKView *view;
  AAPLRenderer *renderer;

  (void)argc, (void)argv;
  NSSetUncaughtExceptionHandler(on_exception);
  if (!device) {
    fprintf(stderr, "triangle: no Metal device\n");
    return 1;
  }
  if (!(view = [[MTKView alloc] initWithFrame:(CGRect){ { 0, 0 }, { 800, 600 } } device:device]))
    return 1;
  view.title = @"Metal (ObjC): AAPLRenderer triangle";
  renderer = [[AAPLRenderer alloc] initWithMetalKitView:view];
  [renderer mtkView:view drawableSizeWillChange:view.drawableSize];
  view.delegate = renderer;
  [view run];

  [renderer release];
  [view release];
  [pool release];
  return 0;
}
