// A spinning, lit, textured cube through PD's ObjC Metal: a texture filled with replaceRegion:, a sampler state,
// a depth buffer from MTKView and a depth-stencil state. Shaders are Indium's texturing test (Apple sample code,
// MIT: vertex_project, fragment_texture). In an AppKit window by default, a GLFW one with --glfw
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// shaders.metal: packed_float4 position, packed_float4 normal, packed_float2 texCoords
typedef struct {
  float position[4];
  float normal[4];
  float texCoords[2];
} Vertex;

// float4x4 modelViewProjection, float4x4 modelView, float3x3 normal (three float4 columns)
typedef struct {
  float mvp[16];
  float mv[16];
  float normal[12];
} Uniforms;

static void mat_mul(float *out, const float *a, const float *b) {
  float r[16];

  for (int c = 0; c < 4; c++) {
    for (int row = 0; row < 4; row++) {
      r[c * 4 + row] = 0;
      for (int k = 0; k < 4; k++) {
        r[c * 4 + row] += a[k * 4 + row] * b[c * 4 + k];
      }
    }
  }
  memcpy(out, r, sizeof(r));
}

static void mat_rotation(float *m, float angle, float x, float y, float z) {
  float c = cosf(angle), s = sinf(angle), t = 1 - c;
  float r[16] = { t * x * x + c,     t * x * y + s * z, t * x * z - s * y, 0, t * x * y - s * z, t * y * y + c,
                  t * y * z + s * x, 0,                 t * x * z + s * y, t * y * z - s * x,  t * z * z + c,
                  0,                 0,                 0,                 0,                  1 };

  memcpy(m, r, sizeof(r));
}

// right-handed, depth in [0, 1] as Metal clips
static void mat_perspective(float *m, float fovy, float aspect, float near, float far) {
  float f = 1 / tanf(fovy / 2);

  memset(m, 0, 16 * sizeof(float));
  m[0] = f / aspect, m[5] = f, m[10] = far / (near - far), m[11] = -1, m[14] = near * far / (near - far);
}

@interface CubeRenderer : NSObject <MTKViewDelegate>
- (instancetype)initWithView:(MTKView *)view;
@end

@implementation CubeRenderer {
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  id<MTLRenderPipelineState> _pipeline;
  id<MTLDepthStencilState> _depth;
  id<MTLTexture> _texture;
  id<MTLSamplerState> _sampler;
  id<MTLBuffer> _vertices, _indices;
  CGSize _size;
  float _angle;
}

- (instancetype)initWithView:(MTKView *)view {
  static const float faces[6][3][3] = {
    // normal, u axis, v axis
    { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },  { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },
    { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } }, { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
    { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
  };
  Vertex v[24];
  uint16_t idx[36];
  uint8_t pixels[256 * 256 * 4];
  NSError *error = nil;

  if (!(self = [super init]))
    return nil;
  _device = view.device;
  _queue = [_device newCommandQueue];

  id<MTLLibrary> library = [_device newDefaultLibrary];
  MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
  pd.vertexFunction = [library newFunctionWithName:@"vertex_project"];
  pd.fragmentFunction = [library newFunctionWithName:@"fragment_texture"];
  pd.colorAttachments[0].pixelFormat = view.colorPixelFormat;
  pd.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
  if (!(_pipeline = [_device newRenderPipelineStateWithDescriptor:pd error:&error])) {
    fprintf(stderr, "texcube: no pipeline state\n");
    return nil;
  }
  [pd release];

  MTLDepthStencilDescriptor *dd = [[MTLDepthStencilDescriptor alloc] init];
  dd.depthCompareFunction = MTLCompareFunctionLess;
  dd.depthWriteEnabled = YES;
  _depth = [_device newDepthStencilStateWithDescriptor:dd];
  [dd release];

  // a checkerboard with a colour ramp, so filtering and orientation both show
  for (int y = 0; y < 256; y++) {
    for (int x = 0; x < 256; x++) {
      uint8_t *p = pixels + (y * 256 + x) * 4;
      int on = ((x >> 5) ^ (y >> 5)) & 1;

      p[0] = on ? 255 : x, p[1] = on ? 255 : y, p[2] = on ? 255 : 128, p[3] = 255;
    }
  }
  MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                width:256
                                                                               height:256
                                                                            mipmapped:NO];
  _texture = [_device newTextureWithDescriptor:td];
  [_texture replaceRegion:MTLRegionMake2D(0, 0, 256, 256) mipmapLevel:0 withBytes:pixels bytesPerRow:256 * 4];

  MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
  sd.minFilter = MTLSamplerMinMagFilterNearest;
  sd.magFilter = MTLSamplerMinMagFilterLinear;
  sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
  _sampler = [_device newSamplerStateWithDescriptor:sd];
  [sd release];

  for (int f = 0; f < 6; f++) {
    const float *n = faces[f][0], *u = faces[f][1], *w = faces[f][2];

    for (int i = 0; i < 4; i++) {
      float su = (i == 1 || i == 2) ? 1 : -1, sv = (i >= 2) ? 1 : -1;
      Vertex *vx = &v[f * 4 + i];

      for (int k = 0; k < 3; k++) {
        vx->position[k] = 0.5f * (n[k] + su * u[k] + sv * w[k]);
        vx->normal[k] = n[k];
      }
      vx->position[3] = 1, vx->normal[3] = 0;
      vx->texCoords[0] = (su + 1) / 2, vx->texCoords[1] = 1 - (sv + 1) / 2;
    }
    uint16_t b = (uint16_t)(f * 4), q[6] = { b, (uint16_t)(b + 1), (uint16_t)(b + 2), b, (uint16_t)(b + 2), (uint16_t)(b + 3) };
    memcpy(idx + f * 6, q, sizeof(q));
  }
  _vertices = [_device newBufferWithBytes:v length:sizeof(v) options:MTLResourceStorageModeShared];
  _indices = [_device newBufferWithBytes:idx length:sizeof(idx) options:MTLResourceStorageModeShared];
  return self;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
  (void)view;
  _size = size;
}

- (void)drawInMTKView:(MTKView *)view {
  float rx[16], ry[16], model[16], viewm[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -2, 1 }, proj[16];
  Uniforms u;

  _angle += 1.0f / 60;
  mat_rotation(rx, _angle * 1.3f, 1, 0, 0);
  mat_rotation(ry, _angle, 0, 1, 0);
  mat_mul(model, rx, ry);
  mat_mul(u.mv, viewm, model);
  mat_perspective(proj, 1.2f, _size.height > 0 ? (float)(_size.width / _size.height) : 1, 0.1f, 100);
  mat_mul(u.mvp, proj, u.mv);
  for (int c = 0; c < 3; c++) {
    for (int r = 0; r < 3; r++) {
      u.normal[c * 4 + r] = u.mv[c * 4 + r];
    }
    u.normal[c * 4 + 3] = 0;
  }

  MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
  if (!pass)
    return;
  id<MTLCommandBuffer> cmd = [_queue commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];
  [enc setViewport:(MTLViewport){ 0, 0, _size.width, _size.height, 0, 1 }];
  [enc setRenderPipelineState:_pipeline];
  [enc setDepthStencilState:_depth];
  [enc setFrontFacingWinding:MTLWindingCounterClockwise];
  [enc setCullMode:MTLCullModeBack];
  [enc setVertexBuffer:_vertices offset:0 atIndex:0];
  [enc setVertexBytes:&u length:sizeof(u) atIndex:1];
  [enc setFragmentBytes:&u length:sizeof(u) atIndex:0];
  [enc setFragmentTexture:_texture atIndex:0];
  [enc setFragmentSamplerState:_sampler atIndex:0];
  [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                  indexCount:36
                   indexType:MTLIndexTypeUInt16
                 indexBuffer:_indices
           indexBufferOffset:0];
  [enc endEncoding];
  [cmd presentDrawable:view.currentDrawable];
  [cmd commit];
}

@end

int main(int argc, char **argv) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  BOOL glfw = argc > 1 && !strcmp(argv[1], "--glfw");
  NSRect frame = NSMakeRect(100, 100, 800, 600);
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  MTKView *view;
  CubeRenderer *renderer;

  if (!device) {
    fprintf(stderr, "texcube: no Metal device\n");
    return 1;
  }
  if (!glfw)
    [NSApplication sharedApplication];
  view = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height) device:device];
  view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
  view.clearColor = MTLClearColorMake(0.1, 0.1, 0.15, 1);
  if (!(renderer = [[CubeRenderer alloc] initWithView:view]))
    return 1;
  [renderer mtkView:view drawableSizeWillChange:view.drawableSize];
  view.delegate = renderer;

  if (glfw) {
    view.title = @"Metal (ObjC): textured cube";
    [view run];
  } else {
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setTitle:@"Metal (ObjC, AppKit): textured cube"];
    [window setContentView:view];
    [window makeKeyAndOrderFront:nil];
    [NSApp run];
  }
  [pool release];
  return 0;
}
