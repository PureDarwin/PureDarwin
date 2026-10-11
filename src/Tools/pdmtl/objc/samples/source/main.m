// newLibraryWithSource: on PureDarwin: a kernel shipped as source in the app's Resources, compiled from the
// metallib cache (metal-precompile), run and checked; then the same source with a macro the cache lacks, which
// has to fail with MTLLibraryErrorCompileFailure naming the key
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>

static int run(id<MTLDevice> device, NSString *source, MTLCompileOptions *options, float scale) {
  NSError *error = nil;
  id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
  enum { N = 1024 };
  float a[N], b[N];
  int bad = 0;

  if (!library) {
    printf("newLibraryWithSource: failed (%s %ld): %s\n", [[error domain] UTF8String], (long)[error code],
           [[error localizedDescription] UTF8String]);
    return -1;
  }
  for (int i = 0; i < N; i++)
    a[i] = (float)i, b[i] = 0.5f * (float)i;

  id<MTLFunction> fn = [library newFunctionWithName:@"scale_add"];
  id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:fn error:&error];
  id<MTLBuffer> ba = [device newBufferWithBytes:a length:sizeof(a) options:MTLResourceStorageModeShared];
  id<MTLBuffer> bb = [device newBufferWithBytes:b length:sizeof(b) options:MTLResourceStorageModeShared];
  id<MTLBuffer> out = [device newBufferWithLength:sizeof(a) options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];

  [enc setComputePipelineState:pipeline];
  [enc setBuffer:ba offset:0 atIndex:0];
  [enc setBuffer:bb offset:0 atIndex:1];
  [enc setBuffer:out offset:0 atIndex:2];
  [enc dispatchThreads:MTLSizeMake(N, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  for (int i = 0; i < N; i++)
    if (((float *)out.contents)[i] != a[i] * scale + b[i])
      bad++;
  printf("newLibraryWithSource: ran scale_add over %d floats, %d wrong (scale %g)\n", N, bad, scale);
  return bad ? -1 : 0;
}

int main(void) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSString *path = [[NSBundle mainBundle] pathForResource:@"add" ofType:@"metal"];
    NSString *source = path ? [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL] : nil;
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    int ok;

    if (!device || !source) {
      printf("no Metal device or no add.metal in the bundle\n");
      return 1;
    }
    ok = run(device, source, nil, 2) == 0;
    // the cache has SCALE=3 too (metal-precompile -D SCALE=3), but not SCALE=5
    options.preprocessorMacros = @{ @"SCALE" : @3 };
    ok &= run(device, source, options, 3) == 0;
    options.preprocessorMacros = @{ @"SCALE" : @5 };
    ok &= run(device, source, options, 5) == -1;
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
  }
}
