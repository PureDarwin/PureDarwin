// newLibraryWithSource: on PureDarwin: a kernel shipped as source in the app's Resources, from the metallib
// cache or compiled by mslc, run and checked with three macro sets, then a vertex and fragment pair drawn
// offscreen through both the sync and the async call
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <objc/message.h>

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

static NSString *const drawSource =
    @"#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "struct VOut { float4 pos [[position]]; float4 color [[user(locn0)]]; };\n"
     "vertex VOut vmain(uint vid [[vertex_id]], constant float2 *pts [[buffer(0)]]) {\n"
     "  VOut o;\n"
     "  o.pos = float4(pts[vid], 0.0, 1.0);\n"
     "  o.color = float4(TINT, 1.0);\n"
     "  return o;\n"
     "}\n"
     "fragment float4 fmain(VOut in [[stage_in]]) { return in.color; }\n";

// one triangle over the left half of a 64x64 target: the left pixel takes the tint, the right one the clear colour.
// The tint macro has no spaces: Apple's compiler splits a macro value at them
// indirect: 1 drawPrimitives:indirectBuffer:, 2 drawIndexedPrimitives:...indirectBuffer:
static int draw(id<MTLDevice> device, BOOL async, int indirect, NSString *tint, const uint8_t want[4]) {
  MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
  __block id<MTLLibrary> library = nil;
  __block NSError *error = nil;
  NSDate *t0 = [NSDate date];

  options.preprocessorMacros = @{ @"TINT" : tint };
  if (async) {
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [device newLibraryWithSource:drawSource options:options completionHandler:^(id<MTLLibrary> l, NSError *e) {
      library = l;
      error = e;
      dispatch_semaphore_signal(done);
    }];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
  } else {
    NSError *e = nil;
    library = [device newLibraryWithSource:drawSource options:options error:&e];
    error = e;
  }
  if (!library) {
    printf("draw: newLibraryWithSource: failed: %s\n", [[error localizedDescription] UTF8String]);
    return -1;
  }
  printf("draw: %s%s newLibraryWithSource: took %.3f s\n", async ? "async" : "sync",
         indirect >= 2 ? ", indexed indirect draw," : indirect ? ", indirect draw," : "",
         -[t0 timeIntervalSinceNow]);

  MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
  pd.vertexFunction = [library newFunctionWithName:@"vmain"];
  pd.fragmentFunction = [library newFunctionWithName:@"fmain"];
  pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
  NSError *perr = nil;
  id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pd error:&perr];
  if (!pipeline) {
    printf("draw: no pipeline: %s\n", [[perr localizedDescription] UTF8String]);
    return -1;
  }
  MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                 width:64 height:64 mipmapped:NO];
  td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  td.storageMode = MTLStorageModePrivate;
  id<MTLTexture> target = [device newTextureWithDescriptor:td];
  float pts[] = { -1, -1, 0, -1, -1, 3, 0, 3 };
  MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
  rp.colorAttachments[0].texture = target;
  rp.colorAttachments[0].loadAction = MTLLoadActionClear;
  rp.colorAttachments[0].storeAction = MTLStoreActionStore;
  rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
  id<MTLCommandQueue> queue = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [queue commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
  [enc setRenderPipelineState:pipeline];
  [enc setVertexBytes:pts length:sizeof(pts) atIndex:0];
  if (indirect == 1) {
    // vertexCount, instanceCount, vertexStart, baseInstance
    uint32_t args[4] = { 3, 1, 0, 0 };
    id<MTLBuffer> ib = [device newBufferWithBytes:args length:sizeof(args) options:MTLResourceStorageModeShared];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:ib indirectBufferOffset:0];
  } else if (indirect >= 2) {
    // indexCount, instanceCount, indexStart, baseVertex, baseInstance: indices 9 .. 11 of the list with baseVertex -1
    // (2), indices 9 .. 11 as 0, 1, 2 (3), indexStart 0 on the same list (4), case 2 drawn directly (5)
    uint32_t args[5] = { 3, 1, indirect == 4 ? 0 : 2, indirect == 2 || indirect == 5 ? (uint32_t)-1 : 0, 0 };
    uint16_t idx[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    if (indirect == 3)
      idx[9] = 0, idx[10] = 1, idx[11] = 2;
    if (indirect == 4)
      idx[7] = 0, idx[8] = 1, idx[9] = 2;
    id<MTLBuffer> ab = [device newBufferWithBytes:args length:sizeof(args) options:MTLResourceStorageModeShared];
    id<MTLBuffer> xb = [device newBufferWithBytes:idx length:sizeof(idx) options:MTLResourceStorageModeShared];
    if (indirect == 5)
      [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:3 indexType:MTLIndexTypeUInt16 indexBuffer:xb
               indexBufferOffset:18 instanceCount:1 baseVertex:-1 baseInstance:0];
    else
      [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexType:MTLIndexTypeUInt16 indexBuffer:xb indexBufferOffset:14
                  indirectBuffer:ab indirectBufferOffset:0];
  } else {
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  }
  [enc endEncoding];
  // read back through a blit into a shared buffer: macOS keeps render targets out of shared storage
  id<MTLBuffer> rb = [device newBufferWithLength:64 * 64 * 4 options:MTLResourceStorageModeShared];
  id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
  [blit copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(64, 64, 1)
               toBuffer:rb destinationOffset:0 destinationBytesPerRow:64 * 4 destinationBytesPerImage:64 * 64 * 4];
  [blit endEncoding];
  [cb commit];
  [cb waitUntilCompleted];

  const uint8_t *px = (const uint8_t *)rb.contents;
  const uint8_t *l = px + (32 * 64 + 8) * 4, *r = px + (32 * 64 + 56) * 4;
  int ok = 1;
  for (int c = 0; c < 4; c++)
    ok &= abs(l[c] - want[c]) <= 1 && r[c] == (c == 3 ? 255 : 0);
  printf("draw: left %d,%d,%d,%d right %d,%d,%d,%d: %s\n", l[0], l[1], l[2], l[3], r[0], r[1], r[2], r[3],
         ok ? "ok" : "WRONG");
  return ok ? 0 : -1;
}

static NSString *const texelSource =
    @"#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "kernel void tb(texture_buffer<int> src [[texture(0)]], texture_buffer<float, access::write> dst [[texture(1)]],\n"
     "               uint i [[thread_position_in_grid]]) {\n"
     "  if (i < src.get_width())\n"
     "    dst.write(float4(float(src.read(i).x) * 0.5, 0.0, 0.0, 1.0), i);\n"
     "}\n";

// texture buffers over MTLBuffers: the kernel reads ints the CPU put in one buffer and writes floats the CPU then
// reads from the other, both through textureBufferDescriptorWithPixelFormat:
static int texel(id<MTLDevice> device) {
  enum { N = 20000 };
  NSError *error = nil;
  id<MTLLibrary> library = [device newLibraryWithSource:texelSource options:nil error:&error];
  id<MTLComputePipelineState> pipeline;
  int bad = 0;

  if (!library) {
    printf("texel: newLibraryWithSource: failed: %s\n", [[error localizedDescription] UTF8String]);
    return -1;
  }
  pipeline = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"tb"] error:&error];
  id<MTLBuffer> in = [device newBufferWithLength:N * 4 options:MTLResourceStorageModeShared];
  id<MTLBuffer> out = [device newBufferWithLength:N * 4 options:MTLResourceStorageModeShared];
  for (int i = 0; i < N; i++)
    ((int *)in.contents)[i] = i - 7;
  MTLTextureDescriptor *di = [MTLTextureDescriptor textureBufferDescriptorWithPixelFormat:MTLPixelFormatR32Sint width:N
                                                                          resourceOptions:MTLResourceStorageModeShared
                                                                                    usage:MTLTextureUsageShaderRead];
  MTLTextureDescriptor *dout = [MTLTextureDescriptor textureBufferDescriptorWithPixelFormat:MTLPixelFormatR32Float width:N
                                                                            resourceOptions:MTLResourceStorageModeShared
                                                                                      usage:MTLTextureUsageShaderWrite];
  id<MTLTexture> ti = [in newTextureWithDescriptor:di offset:0 bytesPerRow:N * 4];
  id<MTLTexture> to = [out newTextureWithDescriptor:dout offset:0 bytesPerRow:N * 4];
  if (!pipeline || !ti || !to || ti.buffer != in || ti.textureType != MTLTextureTypeTextureBuffer) {
    printf("texel: no pipeline or texture buffer\n");
    return -1;
  }
  id<MTLCommandQueue> queue = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
  [enc setComputePipelineState:pipeline];
  [enc setTexture:ti atIndex:0];
  [enc setTexture:to atIndex:1];
  [enc dispatchThreads:MTLSizeMake(N, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  for (int i = 0; i < N; i++)
    if (((float *)out.contents)[i] != (float)(i - 7) * 0.5f)
      bad++;
  printf("texel: %d texture buffer texels through a kernel, %d wrong\n", N, bad);
  return bad ? -1 : 0;
}

static NSString *const fcSource =
    @"#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "constant int K [[function_constant(0)]];\n"
     "constant bool B [[function_constant(1)]];\n"
     "constant int Kv = is_function_constant_defined(K) ? K : 100;\n"
     "kernel void fc(device int *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {\n"
     "  o[i] = (B ? 1000 : 0) + Kv + int(i);\n"
     "}\n";

// function constants through MTLFunctionConstantValues, by message so the same file builds where the class is
// only in the runtime: K = 5 and B = true give 1005 + i, nothing set gives 100 + i
static int fconst(id<MTLDevice> device, int setK, int k, int b, int base, int indirect) {
  enum { N = 256 };
  NSError *error = nil;
  id<MTLLibrary> library = [device newLibraryWithSource:fcSource options:nil error:&error];
  id values = [[NSClassFromString(@"MTLFunctionConstantValues") alloc] init];
  bool bv = b != 0;
  int bad = 0;

  if (!library || !values) {
    printf("fconst: no library or MTLFunctionConstantValues: %s\n", [[error localizedDescription] UTF8String]);
    return -1;
  }
  if (setK)
    ((void (*)(id, SEL, const void *, NSUInteger, NSUInteger))objc_msgSend)(values, @selector(setConstantValue:type:atIndex:),
                                                                          &k, 29 /* MTLDataTypeInt */, 0);
  ((void (*)(id, SEL, const void *, NSUInteger, NSUInteger))objc_msgSend)(values, @selector(setConstantValue:type:atIndex:),
                                                                        &bv, 53 /* MTLDataTypeBool */, 1);
  id<MTLFunction> fn = ((id (*)(id, SEL, NSString *, id, NSError **))objc_msgSend)(
      library, @selector(newFunctionWithName:constantValues:error:), @"fc", values, &error);
  id<MTLComputePipelineState> pipeline = fn ? [device newComputePipelineStateWithFunction:fn error:&error] : nil;
  if (!pipeline) {
    printf("fconst: no function or pipeline: %s\n", [[error localizedDescription] UTF8String]);
    return -1;
  }
  id<MTLBuffer> out = [device newBufferWithLength:N * 4 options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> queue = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [queue commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
  [enc setComputePipelineState:pipeline];
  [enc setBuffer:out offset:0 atIndex:0];
  if (indirect) {
    // threadgroupsPerGrid
    uint32_t groups[3] = { N / 64, 1, 1 };
    id<MTLBuffer> gb = [device newBufferWithBytes:groups length:sizeof(groups) options:MTLResourceStorageModeShared];
    [enc dispatchThreadgroupsWithIndirectBuffer:gb indirectBufferOffset:0 threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
  } else {
    [enc dispatchThreads:MTLSizeMake(N, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
  }
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  for (int i = 0; i < N; i++)
    if (((int *)out.contents)[i] != base + i)
      bad++;
  printf("fconst: K %s, B %d%s: first %d, %d of %d wrong\n", setK ? "set" : "unset", b, indirect ? ", indirect dispatch" : "",
         ((int *)out.contents)[0], bad, N);
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
    // the system cache has SCALE=3 (metal-precompile -D SCALE=3), SCALE=5 is mslc's to compile
    options.preprocessorMacros = @{ @"SCALE" : @3 };
    ok &= run(device, source, options, 3) == 0;
    options.preprocessorMacros = @{ @"SCALE" : @5 };
    ok &= run(device, source, options, 5) == 0;
    ok &= draw(device, NO, 0, @"float3(1.0,0.5,0.0)", (const uint8_t[]){ 255, 128, 0, 255 }) == 0;
    ok &= draw(device, YES, 0, @"float3(0.0,0.25,1.0)", (const uint8_t[]){ 0, 64, 255, 255 }) == 0;
    ok &= draw(device, NO, 1, @"float3(0.5,1.0,0.0)", (const uint8_t[]){ 128, 255, 0, 255 }) == 0;
    ok &= draw(device, NO, 2, @"float3(1.0,1.0,0.0)", (const uint8_t[]){ 255, 255, 0, 255 }) == 0;
    ok &= draw(device, NO, 3, @"float3(0.0,1.0,1.0)", (const uint8_t[]){ 0, 255, 255, 255 }) == 0;
    ok &= draw(device, NO, 4, @"float3(1.0,0.0,1.0)", (const uint8_t[]){ 255, 0, 255, 255 }) == 0;
    // the same indices and base vertex as case 2 through the direct call
    ok &= draw(device, NO, 5, @"float3(0.5,0.5,1.0)", (const uint8_t[]){ 128, 128, 255, 255 }) == 0;
    ok &= texel(device) == 0;
    ok &= fconst(device, 1, 5, 1, 1005, 0) == 0;
    ok &= fconst(device, 0, 0, 0, 100, 1) == 0;
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
  }
}
