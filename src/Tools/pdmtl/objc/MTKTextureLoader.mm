// the MTKTextureLoader for PureDarwin: images decoded by CGImageSource (Onyx2D) into RGBA8, KTX 1 files uploaded level by level,
// mipmaps generated with a blit like Apple's loader. MRC, like the rest of PD's MetalKit
#import <MetalKit/MTKTextureLoader.h>
#import <CoreGraphics/CGImageSource.h>

#include <stdint.h>
#include <string.h>
#include <vector>

MTKTextureLoaderError const MTKTextureLoaderErrorDomain = @"MTKTextureLoaderErrorDomain";
MTKTextureLoaderError const MTKTextureLoaderErrorKey = @"MTKTextureLoaderErrorKey";

MTKTextureLoaderOption const MTKTextureLoaderOptionAllocateMipmaps = @"MTKTextureLoaderOptionAllocateMipmaps";
MTKTextureLoaderOption const MTKTextureLoaderOptionGenerateMipmaps = @"MTKTextureLoaderOptionGenerateMipmaps";
MTKTextureLoaderOption const MTKTextureLoaderOptionSRGB = @"MTKTextureLoaderOptionSRGB";
MTKTextureLoaderOption const MTKTextureLoaderOptionTextureUsage = @"MTKTextureLoaderOptionTextureUsage";
MTKTextureLoaderOption const MTKTextureLoaderOptionTextureCPUCacheMode = @"MTKTextureLoaderOptionTextureCPUCacheMode";
MTKTextureLoaderOption const MTKTextureLoaderOptionTextureStorageMode = @"MTKTextureLoaderOptionTextureStorageMode";
MTKTextureLoaderOption const MTKTextureLoaderOptionCubeLayout = @"MTKTextureLoaderOptionCubeLayout";
MTKTextureLoaderOption const MTKTextureLoaderOptionOrigin = @"MTKTextureLoaderOptionOrigin";
MTKTextureLoaderOption const MTKTextureLoaderOptionLoadAsArray = @"MTKTextureLoaderOptionLoadAsArray";

MTKTextureLoaderCubeLayout const MTKTextureLoaderCubeLayoutVertical = @"MTKTextureLoaderCubeLayoutVertical";

MTKTextureLoaderOrigin const MTKTextureLoaderOriginTopLeft = @"MTKTextureLoaderOriginTopLeft";
MTKTextureLoaderOrigin const MTKTextureLoaderOriginBottomLeft = @"MTKTextureLoaderOriginBottomLeft";
MTKTextureLoaderOrigin const MTKTextureLoaderOriginFlippedVertically = @"MTKTextureLoaderOriginFlippedVertically";

static NSError *loader_error(NSString *fmt, ...) {
  va_list ap;
  NSString *text;

  va_start(ap, fmt);
  text = [[[NSString alloc] initWithFormat:fmt arguments:ap] autorelease];
  va_end(ap);
  return [NSError errorWithDomain:MTKTextureLoaderErrorDomain
                             code:0
                         userInfo:@{ NSLocalizedDescriptionKey : text, MTKTextureLoaderErrorKey : text }];
}

static BOOL option_bool(NSDictionary *options, NSString *key, BOOL fallback) {
  id v = [options objectForKey:key];

  return v ? [v boolValue] : fallback;
}

static NSUInteger full_mip_count(NSUInteger w, NSUInteger h) {
  NSUInteger n = 1, m = w > h ? w : h;

  while (m > 1) {
    m >>= 1;
    n++;
  }
  return n;
}

static BOOL flipped(NSDictionary *options) {
  NSString *origin = [options objectForKey:MTKTextureLoaderOptionOrigin];

  return [origin isEqual:MTKTextureLoaderOriginBottomLeft] || [origin isEqual:MTKTextureLoaderOriginFlippedVertically];
}

// pixels of any 8-bit CGImage as straight (not premultiplied) RGBA, top row first
static bool image_rgba8(CGImageRef image, std::vector<uint8_t> &out) {
  size_t w = CGImageGetWidth(image), h = CGImageGetHeight(image);
  size_t bpc = CGImageGetBitsPerComponent(image), bpp = CGImageGetBitsPerPixel(image), stride = CGImageGetBytesPerRow(image);
  CGBitmapInfo info = CGImageGetBitmapInfo(image);
  CGImageAlphaInfo alpha = CGImageGetAlphaInfo(image);
  CGDataProviderRef provider = CGImageGetDataProvider(image);
  CFDataRef data;
  bool little = (info & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Little;
  size_t comps = bpp / 8;

  out.assign(w * h * 4, 255);
  if (bpc != 8 || !provider || !(comps >= 1 && comps <= 4) || !(data = CGDataProviderCopyData(provider)))
    goto draw;
  if ((size_t)CFDataGetLength(data) < stride * (h - 1) + w * comps) {
    CFRelease(data);
    goto draw;
  }
  {
    const uint8_t *src = CFDataGetBytePtr(data);
    bool first = alpha == kCGImageAlphaFirst || alpha == kCGImageAlphaPremultipliedFirst || alpha == kCGImageAlphaNoneSkipFirst;
    bool has_alpha = alpha == kCGImageAlphaFirst || alpha == kCGImageAlphaLast || alpha == kCGImageAlphaPremultipliedFirst ||
                     alpha == kCGImageAlphaPremultipliedLast || alpha == kCGImageAlphaOnly;
    bool premul = alpha == kCGImageAlphaPremultipliedFirst || alpha == kCGImageAlphaPremultipliedLast;

    for (size_t y = 0; y < h; y++) {
      const uint8_t *s = src + y * stride;
      uint8_t *d = &out[y * w * 4];

      for (size_t x = 0; x < w; x++, s += comps, d += 4) {
        uint8_t p[4] = { s[0], (uint8_t)(comps > 1 ? s[1] : 0), (uint8_t)(comps > 2 ? s[2] : 0), (uint8_t)(comps > 3 ? s[3] : 0) };

        // 32-bit little-endian pixels are stored backwards (BGRA for alpha first)
        if (little && comps == 4) {
          uint8_t t0 = p[0], t1 = p[1];

          p[0] = p[3], p[1] = p[2], p[2] = t1, p[3] = t0;
        }
        if (comps == 1) {
          d[0] = d[1] = d[2] = p[0], d[3] = 255;
        } else if (comps == 2) {
          d[0] = d[1] = d[2] = p[0], d[3] = has_alpha ? p[1] : 255;
        } else if (comps == 3) {
          d[0] = p[0], d[1] = p[1], d[2] = p[2], d[3] = 255;
        } else if (first) {
          d[0] = p[1], d[1] = p[2], d[2] = p[3], d[3] = has_alpha ? p[0] : 255;
        } else {
          d[0] = p[0], d[1] = p[1], d[2] = p[2], d[3] = has_alpha ? p[3] : 255;
        }
        if (premul && d[3] && d[3] != 255) {
          for (int c = 0; c < 3; c++) {
            unsigned v = (d[c] * 255u + d[3] / 2) / d[3];

            d[c] = v > 255 ? 255 : (uint8_t)v;
          }
        }
      }
    }
  }
  CFRelease(data);
  return true;

draw:
  // anything else: let CoreGraphics convert it (premultiplied, so translucent pixels lose a little)
  {
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(out.data(), w, h, 8, w * 4, rgb, kCGImageAlphaPremultipliedLast);

    CGColorSpaceRelease(rgb);
    if (!ctx)
      return false;
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), image);
    CGContextRelease(ctx);
    for (size_t i = 0; i < w * h; i++) {
      uint8_t *d = &out[i * 4];

      if (d[3] && d[3] != 255)
        for (int c = 0; c < 3; c++) {
          unsigned v = (d[c] * 255u + d[3] / 2) / d[3];

          d[c] = v > 255 ? 255 : (uint8_t)v;
        }
    }
  }
  return true;
}

// KTX 1 internal formats this loader knows, with their block size (1x1 for plain formats) and bytes per block
struct KTXFormat {
  uint32_t gl;
  MTLPixelFormat mtl;
  uint32_t block, bytes;
};

static const KTXFormat ktx_formats[] = {
  { 0x8058, MTLPixelFormatRGBA8Unorm, 1, 4 },          // GL_RGBA8
  { 0x8C43, MTLPixelFormatRGBA8Unorm_sRGB, 1, 4 },     // GL_SRGB8_ALPHA8
  { 0x8229, MTLPixelFormatR8Unorm, 1, 1 },             // GL_R8
  { 0x822B, MTLPixelFormatRG8Unorm, 1, 2 },            // GL_RG8
  { 0x822D, MTLPixelFormatR16Float, 1, 2 },            // GL_R16F
  { 0x822F, MTLPixelFormatRG16Float, 1, 4 },           // GL_RG16F
  { 0x881A, MTLPixelFormatRGBA16Float, 1, 8 },         // GL_RGBA16F
  { 0x822E, MTLPixelFormatR32Float, 1, 4 },            // GL_R32F
  { 0x8230, MTLPixelFormatRG32Float, 1, 8 },           // GL_RG32F
  { 0x8814, MTLPixelFormatRGBA32Float, 1, 16 },        // GL_RGBA32F
  { 0x83F1, MTLPixelFormatBC1_RGBA, 4, 8 },            // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
  { 0x8C4D, MTLPixelFormatBC1_RGBA_sRGB, 4, 8 },
  { 0x83F2, MTLPixelFormatBC2_RGBA, 4, 16 },
  { 0x8C4E, MTLPixelFormatBC2_RGBA_sRGB, 4, 16 },
  { 0x83F3, MTLPixelFormatBC3_RGBA, 4, 16 },
  { 0x8C4F, MTLPixelFormatBC3_RGBA_sRGB, 4, 16 },
  { 0x8DBB, MTLPixelFormatBC4_RUnorm, 4, 8 },          // GL_COMPRESSED_RED_RGTC1
  { 0x8DBD, MTLPixelFormatBC5_RGUnorm, 4, 16 },        // GL_COMPRESSED_RG_RGTC2
  { 0x8E8C, MTLPixelFormatBC7_RGBAUnorm, 4, 16 },      // GL_COMPRESSED_RGBA_BPTC_UNORM
  { 0x8E8D, MTLPixelFormatBC7_RGBAUnorm_sRGB, 4, 16 },
  { 0x8E8F, MTLPixelFormatBC6H_RGBUfloat, 4, 16 },
  { 0x8E8E, MTLPixelFormatBC6H_RGBFloat, 4, 16 },
  { 0x9274, MTLPixelFormatETC2_RGB8, 4, 8 },
  { 0x9278, MTLPixelFormatEAC_RGBA8, 4, 16 },
  { 0x93B0, MTLPixelFormatASTC_4x4_LDR, 4, 16 },
  { 0x93D0, MTLPixelFormatASTC_4x4_sRGB, 4, 16 },
};

static MTLPixelFormat linear_format(MTLPixelFormat f) {
  switch (f) {
  case MTLPixelFormatRGBA8Unorm_sRGB: return MTLPixelFormatRGBA8Unorm;
  case MTLPixelFormatBC1_RGBA_sRGB: return MTLPixelFormatBC1_RGBA;
  case MTLPixelFormatBC2_RGBA_sRGB: return MTLPixelFormatBC2_RGBA;
  case MTLPixelFormatBC3_RGBA_sRGB: return MTLPixelFormatBC3_RGBA;
  case MTLPixelFormatBC7_RGBAUnorm_sRGB: return MTLPixelFormatBC7_RGBAUnorm;
  case MTLPixelFormatASTC_4x4_sRGB: return MTLPixelFormatASTC_4x4_LDR;
  default: return f;
  }
}

static bool is_ktx1(NSData *data) {
  static const uint8_t magic[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };

  return data.length >= 64 && !memcmp(data.bytes, magic, sizeof(magic));
}

@implementation MTKTextureLoader {
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
}

@synthesize device = _device;

- (instancetype)initWithDevice:(id<MTLDevice>)device {
  if (!(self = [super init]))
    return nil;
  _device = [device retain];
  return self;
}

- (void)dealloc {
  [_queue release];
  [_device release];
  [super dealloc];
}

- (MTLTextureDescriptor *)descriptorWithFormat:(MTLPixelFormat)format
                                         width:(NSUInteger)w
                                        height:(NSUInteger)h
                                        levels:(NSUInteger)levels
                                       options:(NSDictionary *)options {
  MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
  id usage = [options objectForKey:MTKTextureLoaderOptionTextureUsage];
  id storage = [options objectForKey:MTKTextureLoaderOptionTextureStorageMode];
  id cache = [options objectForKey:MTKTextureLoaderOptionTextureCPUCacheMode];

  desc.mipmapLevelCount = levels;
  desc.usage = usage ? (MTLTextureUsage)[usage unsignedIntegerValue] : MTLTextureUsageShaderRead;
  if (storage)
    desc.storageMode = (MTLStorageMode)[storage unsignedIntegerValue];
  if (cache)
    desc.cpuCacheMode = (MTLCPUCacheMode)[cache unsignedIntegerValue];
  return desc;
}

- (void)generateMipmaps:(id<MTLTexture>)texture {
  id<MTLCommandBuffer> cb;
  id<MTLBlitCommandEncoder> blit;

  if (!_queue)
    _queue = [_device newCommandQueue];
  cb = [_queue commandBuffer];
  blit = [cb blitCommandEncoder];
  [blit generateMipmapsForTexture:texture];
  [blit endEncoding];
  [cb commit];
  [cb waitUntilCompleted];
}

- (id<MTLTexture>)newTextureWithCGImage:(CGImageRef)image options:(NSDictionary *)options error:(NSError **)error {
  std::vector<uint8_t> pixels;
  size_t w, h;
  BOOL gen = option_bool(options, MTKTextureLoaderOptionGenerateMipmaps, NO);
  BOOL alloc = gen || option_bool(options, MTKTextureLoaderOptionAllocateMipmaps, NO);
  MTLPixelFormat format;
  id<MTLTexture> texture;

  if (!image) {
    if (error)
      *error = loader_error(@"No image to load");
    return nil;
  }
  w = CGImageGetWidth(image), h = CGImageGetHeight(image);
  if (!image_rgba8(image, pixels)) {
    if (error)
      *error = loader_error(@"Could not convert a %zux%zu image to RGBA8", w, h);
    return nil;
  }
  if (flipped(options)) {
    std::vector<uint8_t> row(w * 4);

    for (size_t y = 0; y < h / 2; y++) {
      memcpy(row.data(), &pixels[y * w * 4], w * 4);
      memcpy(&pixels[y * w * 4], &pixels[(h - 1 - y) * w * 4], w * 4);
      memcpy(&pixels[(h - 1 - y) * w * 4], row.data(), w * 4);
    }
  }
  // as on macOS, 8-bit images are sRGB unless the caller says otherwise
  format = option_bool(options, MTKTextureLoaderOptionSRGB, YES) ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm;
  texture = [_device newTextureWithDescriptor:[self descriptorWithFormat:format
                                                                   width:w
                                                                  height:h
                                                                  levels:alloc ? full_mip_count(w, h) : 1
                                                                 options:options]];
  if (!texture) {
    if (error)
      *error = loader_error(@"Could not create a %zux%zu texture", w, h);
    return nil;
  }
  [texture replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:w * 4];
  if (gen && texture.mipmapLevelCount > 1)
    [self generateMipmaps:texture];
  return texture;
}

- (id<MTLTexture>)newTextureWithKTX:(NSData *)data options:(NSDictionary *)options error:(NSError **)error {
  const uint8_t *p = (const uint8_t *)data.bytes, *end = p + data.length;
  uint32_t hdr[13];
  const KTXFormat *fmt = NULL;
  id<MTLTexture> texture;
  MTLTextureDescriptor *desc;
  bool swap;
  uint32_t w, h, faces, levels, arrays;

  memcpy(hdr, p + 12, sizeof(hdr));
  swap = hdr[0] == 0x01020304;
  if (swap)
    for (int i = 0; i < 13; i++)
      hdr[i] = __builtin_bswap32(hdr[i]);
  for (size_t i = 0; i < sizeof(ktx_formats) / sizeof(ktx_formats[0]); i++)
    if (ktx_formats[i].gl == hdr[4])
      fmt = &ktx_formats[i];
  if (!fmt) {
    if (error)
      *error = loader_error(@"KTX internal format 0x%x is not supported", hdr[4]);
    return nil;
  }
  w = hdr[6], h = hdr[7] ? hdr[7] : 1, arrays = hdr[9], faces = hdr[10], levels = hdr[11] ? hdr[11] : 1;
  if (hdr[8] > 1 || arrays > 1 || (faces != 1 && faces != 6)) {
    if (error)
      *error = loader_error(@"KTX: 3D and array textures are not supported");
    return nil;
  }
  // sRGB off picks the linear twin of an sRGB format
  MTLPixelFormat format = option_bool(options, MTKTextureLoaderOptionSRGB, YES) ? fmt->mtl : linear_format(fmt->mtl);

  BOOL gen = option_bool(options, MTKTextureLoaderOptionGenerateMipmaps, NO) && levels == 1;
  NSUInteger count = gen || option_bool(options, MTKTextureLoaderOptionAllocateMipmaps, NO) ? full_mip_count(w, h) : levels;

  desc = [self descriptorWithFormat:format width:w height:h levels:count options:options];
  if (faces == 6)
    desc.textureType = MTLTextureTypeCube;
  if (!(texture = [_device newTextureWithDescriptor:desc])) {
    if (error)
      *error = loader_error(@"Could not create a %ux%u texture for KTX format 0x%x", w, h, hdr[4]);
    return nil;
  }
  p += 64 + hdr[12];
  for (uint32_t level = 0; level < levels && level < count; level++) {
    uint32_t lw = w >> level ? w >> level : 1, lh = h >> level ? h >> level : 1, size;
    uint32_t bw = (lw + fmt->block - 1) / fmt->block, bh = (lh + fmt->block - 1) / fmt->block;

    if (p + 4 > end)
      break;
    memcpy(&size, p, 4);
    if (swap)
      size = __builtin_bswap32(size);
    p += 4;
    for (uint32_t face = 0; face < faces; face++) {
      size_t face_bytes = (size_t)bw * bh * fmt->bytes;

      if (p + face_bytes > end) {
        [texture release];
        if (error)
          *error = loader_error(@"KTX file is truncated at level %u", level);
        return nil;
      }
      [texture replaceRegion:MTLRegionMake2D(0, 0, lw, lh)
                 mipmapLevel:level
                       slice:face
                   withBytes:p
                 bytesPerRow:(size_t)bw * fmt->bytes
               bytesPerImage:face_bytes];
      p += (face_bytes + 3) & ~(size_t)3;
    }
  }
  if (gen && count > 1)
    [self generateMipmaps:texture];
  return texture;
}

- (id<MTLTexture>)newTextureWithData:(NSData *)data options:(NSDictionary *)options error:(NSError **)error {
  CGImageSourceRef source;
  CGImageRef image;
  id<MTLTexture> texture;

  if (is_ktx1(data))
    return [self newTextureWithKTX:data options:options error:error];
  // PD's CGImageSource is an Onyx2D object that CFRelease would halt on, so it is released as one
  if (!(source = CGImageSourceCreateWithData((CFDataRef)data, NULL)) ||
      !(image = CGImageSourceCreateImageAtIndex(source, 0, NULL))) {
    if (source)
      [(id)source release];
    if (error)
      *error = loader_error(@"Image data (%lu bytes) is in no format CGImageSource or the KTX reader knows",
                            (unsigned long)data.length);
    return nil;
  }
  texture = [self newTextureWithCGImage:image options:options error:error];
  CGImageRelease(image);
  [(id)source release];
  return texture;
}

- (id<MTLTexture>)newTextureWithContentsOfURL:(NSURL *)URL options:(NSDictionary *)options error:(NSError **)error {
  NSError *readError = nil;
  NSData *data = URL ? [NSData dataWithContentsOfURL:URL options:0 error:&readError] : nil;
  id<MTLTexture> texture;

  if (!data) {
    if (error)
      *error = loader_error(@"Could not read %@%@%@", URL, readError ? @": " : @"", readError ? [readError localizedDescription] : @"");
    return nil;
  }
  texture = [self newTextureWithData:data options:options error:error];
  if (!texture && error && *error)
    *error = loader_error(@"%@: %@", [URL lastPathComponent], [*error localizedDescription]);
  return texture;
}

- (NSArray *)newTexturesWithContentsOfURLs:(NSArray *)URLs options:(NSDictionary *)options error:(NSError **)error {
  NSMutableArray *textures = [[NSMutableArray alloc] init];

  for (NSURL *url in URLs) {
    id<MTLTexture> t = [self newTextureWithContentsOfURL:url options:options error:error];

    if (!t) {
      [textures release];
      return nil;
    }
    [textures addObject:t];
    [t release];
  }
  return textures;
}

// asset catalogs are not compiled on PD: the build copies their images into Resources under the asset's name
- (id<MTLTexture>)newTextureWithName:(NSString *)name
                         scaleFactor:(CGFloat)scaleFactor
                              bundle:(NSBundle *)bundle
                             options:(NSDictionary *)options
                               error:(NSError **)error {
  NSArray *types = @[ @"ktx", @"png", @"jpg", @"jpeg", @"tiff", @"tif", @"bmp", @"gif" ];
  NSString *path = nil;

  if (!bundle)
    bundle = [NSBundle mainBundle];
  for (NSString *type in types) {
    if (scaleFactor > 1 && (path = [bundle pathForResource:[NSString stringWithFormat:@"%@@%dx", name, (int)scaleFactor] ofType:type]))
      break;
    if ((path = [bundle pathForResource:name ofType:type]))
      break;
  }
  if (!path) {
    if (error)
      *error = loader_error(@"No image named %@ in %@ (PD has no asset catalogs: expected %@.png, .jpg or .ktx in Resources)",
                            name, [bundle bundlePath], name);
    return nil;
  }
  return [self newTextureWithContentsOfURL:[NSURL fileURLWithPath:path] options:options error:error];
}

- (void)newTextureWithContentsOfURL:(NSURL *)URL options:(NSDictionary *)options completionHandler:(MTKTextureLoaderCallback)handler {
  NSError *error = nil;
  id<MTLTexture> t = [self newTextureWithContentsOfURL:URL options:options error:&error];

  handler(t, error);
  [t release];
}

- (void)newTexturesWithContentsOfURLs:(NSArray *)URLs options:(NSDictionary *)options completionHandler:(MTKTextureLoaderArrayCallback)handler {
  NSError *error = nil;
  NSArray *t = [self newTexturesWithContentsOfURLs:URLs options:options error:&error];

  handler(t, error);
  [t release];
}

- (void)newTextureWithData:(NSData *)data options:(NSDictionary *)options completionHandler:(MTKTextureLoaderCallback)handler {
  NSError *error = nil;
  id<MTLTexture> t = [self newTextureWithData:data options:options error:&error];

  handler(t, error);
  [t release];
}

- (void)newTextureWithCGImage:(CGImageRef)image options:(NSDictionary *)options completionHandler:(MTKTextureLoaderCallback)handler {
  NSError *error = nil;
  id<MTLTexture> t = [self newTextureWithCGImage:image options:options error:&error];

  handler(t, error);
  [t release];
}

- (void)newTextureWithName:(NSString *)name
               scaleFactor:(CGFloat)scaleFactor
                    bundle:(NSBundle *)bundle
                   options:(NSDictionary *)options
         completionHandler:(MTKTextureLoaderCallback)handler {
  NSError *error = nil;
  id<MTLTexture> t = [self newTextureWithName:name scaleFactor:scaleFactor bundle:bundle options:options error:&error];

  handler(t, error);
  [t release];
}

- (void)newTexturesWithNames:(NSArray *)names
                 scaleFactor:(CGFloat)scaleFactor
                      bundle:(NSBundle *)bundle
                     options:(NSDictionary *)options
           completionHandler:(MTKTextureLoaderArrayCallback)handler {
  NSMutableArray *textures = [NSMutableArray array];
  NSError *error = nil;

  for (NSString *name in names) {
    id<MTLTexture> t = [self newTextureWithName:name scaleFactor:scaleFactor bundle:bundle options:options error:&error];

    if (!t) {
      handler(nil, error);
      return;
    }
    [textures addObject:t];
    [t release];
  }
  handler(textures, nil);
}

@end
