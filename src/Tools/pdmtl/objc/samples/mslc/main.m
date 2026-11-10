// main.m - metal-mslc-test: newLibraryWithSource: compiled in the guest by mslc
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// a kernel over a buffer and a vertex and fragment pair drawing offscreen, checked on the CPU, then a broken
// source has to fail with MTLLibraryErrorCompileFailure and the compiler's message
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

static NSString *const source = @
  "#include <metal_stdlib>\n"
  "using namespace metal;\n"
  "\n"
  "struct Params { float scale; uint n; };\n"
  "\n"
  "kernel void saxpy(device const float *x [[buffer(0)]], device float *y [[buffer(1)]],\n"
  "                  constant Params &p [[buffer(2)]], uint i [[thread_position_in_grid]]) {\n"
  "  if (i < p.n)\n"
  "    y[i] = fma(p.scale, x[i], y[i]) + sqrt(float(i));\n"
  "}\n"
  "\n"
  "struct VOut { float4 position [[position]]; float4 color; };\n"
  "\n"
  "vertex VOut tri_vertex(uint vid [[vertex_id]], constant float2 *pos [[buffer(0)]]) {\n"
  "  const float4 colors[3] = { float4(1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, 0, 1, 1) };\n"
  "  VOut o;\n"
  "  o.position = float4(pos[vid], 0, 1);\n"
  "  o.color = colors[vid];\n"
  "  return o;\n"
  "}\n"
  "\n"
  "fragment float4 tri_fragment(VOut in [[stage_in]], constant float4 &tint [[buffer(0)]]) {\n"
  "  return in.color * tint;\n"
  "}\n"
  "\n"
  "struct QOut { float4 position [[position]]; float2 uv; };\n"
  "\n"
  "vertex QOut quad_vertex(uint vid [[vertex_id]]) {\n"
  "  float2 p = float2((vid << 1) & 2, vid & 2);\n"
  "  QOut o;\n"
  "  o.position = float4(p * 2.0 - 1.0, 0, 1);\n"
  "  o.uv = float2(p.x, 1.0 - p.y);\n"
  "  return o;\n"
  "}\n"
  "\n"
  "fragment half4 quad_fragment(QOut in [[stage_in]], texture2d<half> tex [[texture(0)]]) {\n"
  "  constexpr sampler nearest(filter::nearest, address::clamp_to_edge);\n"
  "  return tex.sample(nearest, in.uv);\n"
  "}\n"
  "\n"
  "constant uint fc_count [[function_constant(0)]];\n"
  "constant bool fc_flag [[function_constant(1)]];\n"
  "constant uint fc_used = is_function_constant_defined(fc_count) ? fc_count : 7;\n"
  "\n"
  "kernel void fc_kernel(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]]) {\n"
  "  if (i == 0) {\n"
  "    out[0] = fc_used;\n"
  "    out[1] = is_function_constant_defined(fc_flag) ? (fc_flag ? 1 : 2) : 3;\n"
  "  }\n"
  "}\n";

static double now(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1e6;
}

static int kernel_check(id<MTLDevice> device, id<MTLLibrary> library) {
  enum { N = 4096 };
  struct { float scale; uint32_t n; } params = { 2.5f, N - 7 };
  float x[N], y[N];
  NSError *error = nil;
  int bad = 0;

  for (int i = 0; i < N; i++)
    x[i] = (float)i * 0.25f, y[i] = 1.0f - (float)i;
  id<MTLComputePipelineState> ps = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"saxpy"]
                                                                         error:&error];
  if (!ps) {
    printf("saxpy pipeline failed: %s\n", error.localizedDescription.UTF8String);
    return 1;
  }
  id<MTLBuffer> bx = [device newBufferWithBytes:x length:sizeof(x) options:MTLResourceStorageModeShared];
  id<MTLBuffer> by = [device newBufferWithBytes:y length:sizeof(y) options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> q = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [q commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
  [enc setComputePipelineState:ps];
  [enc setBuffer:bx offset:0 atIndex:0];
  [enc setBuffer:by offset:0 atIndex:1];
  [enc setBytes:&params length:sizeof(params) atIndex:2];
  [enc dispatchThreads:MTLSizeMake(N, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];

  const float *r = by.contents;
  for (int i = 0; i < N; i++) {
    float want = i < (int)params.n ? fmaf(params.scale, x[i], y[i]) + sqrtf((float)i) : y[i];
    if (fabsf(r[i] - want) > 1e-3f * fmaxf(1.0f, fabsf(want))) {
      if (bad < 4)
        printf("  saxpy[%d] = %g, want %g\n", i, r[i], want);
      bad++;
    }
  }
  printf("kernel saxpy: %d of %d wrong\n", bad, N);
  return bad != 0;
}

static int draw_check(id<MTLDevice> device, id<MTLLibrary> library) {
  enum { W = 64, H = 64 };
  const float pos[3][2] = { { -1, -1 }, { 1, -1 }, { 0, 1 } };
  const float tint[4] = { 1, 1, 1, 1 };
  NSError *error = nil;
  uint8_t px[W * H * 4];
  int bad = 0;

  MTLRenderPipelineDescriptor *rpd = [[MTLRenderPipelineDescriptor alloc] init];
  rpd.vertexFunction = [library newFunctionWithName:@"tri_vertex"];
  rpd.fragmentFunction = [library newFunctionWithName:@"tri_fragment"];
  rpd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
  id<MTLRenderPipelineState> ps = [device newRenderPipelineStateWithDescriptor:rpd error:&error];
  if (!ps) {
    printf("triangle pipeline failed: %s\n", error.localizedDescription.UTF8String);
    return 1;
  }
  MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                 width:W height:H mipmapped:NO];
  td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  td.storageMode = MTLStorageModeShared;
  id<MTLTexture> tex = [device newTextureWithDescriptor:td];

  MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = tex;
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

  id<MTLCommandQueue> q = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [q commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:pass];
  [enc setRenderPipelineState:ps];
  [enc setVertexBytes:pos length:sizeof(pos) atIndex:0];
  [enc setFragmentBytes:tint length:sizeof(tint) atIndex:0];
  [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  [tex getBytes:px bytesPerRow:W * 4 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];

  // the corners stay the clear colour, the middle is covered and mixes all three vertex colours
  const uint8_t *tl = px, *mid = px + ((H / 2) * W + W / 2) * 4;
  if (tl[0] || tl[1] || tl[2] || tl[3] != 255)
    bad++, printf("  corner %d %d %d %d, want 0 0 0 255\n", tl[0], tl[1], tl[2], tl[3]);
  if (mid[0] < 20 || mid[1] < 20 || mid[2] < 20 || mid[3] != 255)
    bad++, printf("  middle %d %d %d %d, want a mix of red, green and blue\n", mid[0], mid[1], mid[2], mid[3]);
  printf("triangle: corner %d %d %d, middle %d %d %d, %s\n", tl[0], tl[1], tl[2], mid[0], mid[1], mid[2],
         bad ? "WRONG" : "ok");

  FILE *f = fopen("/var/tmp/metal-mslc-test.ppm", "wb");
  if (f) {
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++)
      fwrite(px + i * 4, 1, 3, f);
    fclose(f);
  }
  return bad != 0;
}

// a 2x2 texture drawn over the target through a constexpr nearest sampler: each quadrant one texel
static int sample_check(id<MTLDevice> device, id<MTLLibrary> library) {
  enum { W = 64, H = 64 };
  const uint8_t texels[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 } };
  NSError *error = nil;
  uint8_t px[W * H * 4];
  int bad = 0;

  MTLTextureDescriptor *sd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                 width:2 height:2 mipmapped:NO];
  id<MTLTexture> src = [device newTextureWithDescriptor:sd];
  [src replaceRegion:MTLRegionMake2D(0, 0, 2, 2) mipmapLevel:0 withBytes:texels bytesPerRow:8];

  MTLRenderPipelineDescriptor *rpd = [[MTLRenderPipelineDescriptor alloc] init];
  rpd.vertexFunction = [library newFunctionWithName:@"quad_vertex"];
  rpd.fragmentFunction = [library newFunctionWithName:@"quad_fragment"];
  rpd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
  id<MTLRenderPipelineState> ps = [device newRenderPipelineStateWithDescriptor:rpd error:&error];
  if (!ps) {
    printf("quad pipeline failed: %s\n", error.localizedDescription.UTF8String);
    return 1;
  }
  MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                 width:W height:H mipmapped:NO];
  td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  td.storageMode = MTLStorageModeShared;
  id<MTLTexture> tex = [device newTextureWithDescriptor:td];
  MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = tex;
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
  id<MTLCommandQueue> q = [device newCommandQueue];
  id<MTLCommandBuffer> cb = [q commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:pass];
  [enc setRenderPipelineState:ps];
  [enc setFragmentTexture:src atIndex:0];
  [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  [tex getBytes:px bytesPerRow:W * 4 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];

  // texel (0,0) top left, (1,0) top right, (0,1) bottom left, (1,1) bottom right
  const int at[4][2] = { { W / 4, H / 4 }, { 3 * W / 4, H / 4 }, { W / 4, 3 * H / 4 }, { 3 * W / 4, 3 * H / 4 } };
  for (int i = 0; i < 4; i++) {
    const uint8_t *p = px + (at[i][1] * W + at[i][0]) * 4;
    if (memcmp(p, texels[i], 4)) {
      bad++;
      printf("  quadrant %d: %d %d %d %d, want %d %d %d %d\n", i, p[0], p[1], p[2], p[3], texels[i][0], texels[i][1],
             texels[i][2], texels[i][3]);
    }
  }
  printf("constexpr sampler quad: %s\n", bad ? "WRONG" : "ok");
  return bad != 0;
}

// function constants: unset gives the initialiser's default (7) and "not defined", set gives the values
static int fc_run(id<MTLDevice> device, id<MTLLibrary> library, MTLFunctionConstantValues *values, uint32_t want0,
                  uint32_t want1, const char *what) {
  NSError *error = nil;
  id<MTLFunction> fn = values ? [library newFunctionWithName:@"fc_kernel" constantValues:values error:&error]
                              : [library newFunctionWithName:@"fc_kernel"];
  id<MTLComputePipelineState> ps = fn ? [device newComputePipelineStateWithFunction:fn error:&error] : nil;
  if (!ps) {
    printf("function constants (%s): no pipeline: %s\n", what, error.localizedDescription.UTF8String);
    return 1;
  }
  id<MTLBuffer> out = [device newBufferWithLength:8 options:MTLResourceStorageModeShared];
  memset(out.contents, 0xff, 8);
  id<MTLCommandBuffer> cb = [[device newCommandQueue] commandBuffer];
  id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
  [enc setComputePipelineState:ps];
  [enc setBuffer:out offset:0 atIndex:0];
  [enc dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
  [enc endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
  const uint32_t *r = out.contents;
  int bad = r[0] != want0 || r[1] != want1;
  printf("function constants (%s): %u %u, want %u %u, %s\n", what, r[0], r[1], want0, want1, bad ? "WRONG" : "ok");
  return bad;
}

static int fc_check(id<MTLDevice> device, id<MTLLibrary> library) {
  MTLFunctionConstantValues *v = [[MTLFunctionConstantValues alloc] init];
  uint32_t count = 42;
  bool flag = true;
  int fails = fc_run(device, library, nil, 7, 3, "unset");

  [v setConstantValue:&count type:MTLDataTypeUInt atIndex:0];
  [v setConstantValue:&flag type:MTLDataTypeBool withName:@"fc_flag"];
  return fails + fc_run(device, library, v, 42, 1, "set");
}

int main(void) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    NSError *error = nil;
    int fails = 0;

    if (!device) {
      printf("no Metal device\n");
      return 1;
    }
    // a fresh key every run, so this compiles and misses the compiled cache
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.preprocessorMacros = @{ @"PD_MSLC_RUN" : [NSString stringWithFormat:@"%d_%f", getpid(), now()] };

    double t = now();
    id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
    if (!library) {
      printf("newLibraryWithSource: failed (%ld): %s\n", (long)error.code, error.localizedDescription.UTF8String);
      printf("FAIL\n");
      return 1;
    }
    printf("compiled in %.0f ms, functions: %s\n", (now() - t) * 1000,
           [[library.functionNames componentsJoinedByString:@" "] UTF8String]);

    if (library.functionNames.count != 6) {
      printf("functionNames: %lu names, want 6\n", (unsigned long)library.functionNames.count);
      fails++;
    }

    t = now();
    id<MTLLibrary> again = [device newLibraryWithSource:source options:options error:&error];
    printf("same source again (compiled cache): %s in %.0f ms\n", again ? "ok" : "FAILED", (now() - t) * 1000);
    fails += !again;

    fails += kernel_check(device, library);
    fails += draw_check(device, library);
    fails += sample_check(device, library);
    fails += fc_check(device, library);

    id<MTLLibrary> broken = [device newLibraryWithSource:@"kernel void k(device float *p [[buffer(0)]]) { p[0] = undefined_name; }"
                                                 options:nil
                                                   error:&error];
    BOOL reported = !broken && error.code == MTLLibraryErrorCompileFailure &&
                    [error.localizedDescription rangeOfString:@"undefined_name"].location != NSNotFound;
    printf("broken source: %s\n", reported ? "rejected with the compiler's message" : "NOT REPORTED");
    if (!reported && error)
      printf("  %s\n", error.localizedDescription.UTF8String);
    fails += !reported;

    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
  }
}
