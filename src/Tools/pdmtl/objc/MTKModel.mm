// MTKModel.mm - ModelIO meshes in Metal buffers, and Metal textures from ModelIO textures
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <MetalKit/MTKModel.h>
#import <MetalKit/MTKView.h>
#include <string.h>

NSString * const MTKModelErrorDomain = @"MTKModelErrorDomain";
NSString * const MTKModelErrorKey = @"MTKModelErrorKey";

static NSError *model_error(NSString *reason) {
  return [NSError errorWithDomain:MTKModelErrorDomain code:-1
                         userInfo:@{ NSLocalizedDescriptionKey: reason, MTKModelErrorKey: reason }];
}

@interface MTKMeshBuffer ()
- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer length:(NSUInteger)length allocator:(MTKMeshBufferAllocator *)allocator
                          type:(MDLMeshBufferType)type;
@end

@implementation MTKMeshBufferAllocator {
  id<MTLDevice> _device;
}

@synthesize device = _device;

- (instancetype)initWithDevice:(id<MTLDevice>)device {
  if ((self = [super init]))
    _device = [device retain];
  return self;
}

- (void)dealloc {
  [_device release];
  [super dealloc];
}

- (id<MDLMeshBufferZone>)newZone:(NSUInteger)capacity {
  return nil;
}

- (id<MDLMeshBufferZone>)newZoneForBuffersWithSize:(NSArray<NSNumber *> *)sizes andType:(NSArray<NSNumber *> *)types {
  return nil;
}

// shared storage, so ModelIO and the CPU see the same bytes the GPU reads
- (id<MDLMeshBuffer>)newBuffer:(NSUInteger)length type:(MDLMeshBufferType)type {
  id<MTLBuffer> buffer = [_device newBufferWithLength:length ? length : 4 options:MTLResourceStorageModeShared];

  if (!buffer)
    return nil;
  MTKMeshBuffer *result = [[MTKMeshBuffer alloc] initWithBuffer:buffer length:length allocator:self type:type];
  [buffer release];
  return result;
}

- (id<MDLMeshBuffer>)newBufferWithData:(NSData *)data type:(MDLMeshBufferType)type {
  MTKMeshBuffer *buffer = (MTKMeshBuffer *)[self newBuffer:[data length] type:type];

  if (buffer && [data length])
    memcpy([buffer.buffer contents], [data bytes], [data length]);
  return buffer;
}

- (id<MDLMeshBuffer>)newBufferFromZone:(id<MDLMeshBufferZone>)zone length:(NSUInteger)length type:(MDLMeshBufferType)type {
  return [self newBuffer:length type:type];
}

- (id<MDLMeshBuffer>)newBufferFromZone:(id<MDLMeshBufferZone>)zone data:(NSData *)data type:(MDLMeshBufferType)type {
  return [self newBufferWithData:data type:type];
}

@end

@implementation MTKMeshBuffer {
  id<MTLBuffer> _buffer;
  NSUInteger _length;
  MTKMeshBufferAllocator *_allocator;
  MDLMeshBufferType _type;
}

@synthesize buffer = _buffer, length = _length, allocator = _allocator, type = _type;

- (instancetype)initWithBuffer:(id<MTLBuffer>)buffer length:(NSUInteger)length allocator:(MTKMeshBufferAllocator *)allocator
                          type:(MDLMeshBufferType)type {
  if ((self = [super init])) {
    _buffer = [buffer retain];
    _length = length;
    _allocator = [allocator retain];
    _type = type;
  }
  return self;
}

- (void)dealloc {
  [_buffer release];
  [_allocator release];
  [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
  NSData *data = [NSData dataWithBytes:[_buffer contents] length:_length];
  return [_allocator newBufferWithData:data type:_type];
}

- (NSUInteger)offset {
  return 0;
}

- (id<MDLMeshBufferZone>)zone {
  return nil;
}

- (void)fillData:(NSData *)data offset:(NSUInteger)offset {
  NSUInteger length = [data length];

  if (offset >= _length)
    return;
  if (length > _length - offset)
    length = _length - offset;
  memcpy((uint8_t *)[_buffer contents] + offset, [data bytes], length);
}

- (MDLMeshBufferMap *)map {
  id<MTLBuffer> keep = [_buffer retain];
  return [[[MDLMeshBufferMap alloc] initWithBytes:[_buffer contents] deallocator:^{ [keep release]; }] autorelease];
}

@end

@interface MTKSubmesh ()
- (instancetype)initWithSubmesh:(MDLSubmesh *)submesh indexBuffer:(MTKMeshBuffer *)indexBuffer indexType:(MTLIndexType)indexType
                           mesh:(MTKMesh *)mesh;
@end

@implementation MTKSubmesh {
  MTLPrimitiveType _primitiveType;
  MTLIndexType _indexType;
  MTKMeshBuffer *_indexBuffer;
  NSUInteger _indexCount;
  MTKMesh *_mesh;
  NSString *_name;
}

@synthesize primitiveType = _primitiveType, indexType = _indexType, indexBuffer = _indexBuffer, indexCount = _indexCount, mesh = _mesh;

- (instancetype)initWithSubmesh:(MDLSubmesh *)submesh indexBuffer:(MTKMeshBuffer *)indexBuffer indexType:(MTLIndexType)indexType
                           mesh:(MTKMesh *)mesh {
  if ((self = [super init])) {
    switch (submesh.geometryType) {
    case MDLGeometryTypePoints: _primitiveType = MTLPrimitiveTypePoint; break;
    case MDLGeometryTypeLines: _primitiveType = MTLPrimitiveTypeLine; break;
    case MDLGeometryTypeTriangleStrips: _primitiveType = MTLPrimitiveTypeTriangleStrip; break;
    default: _primitiveType = MTLPrimitiveTypeTriangle; break;
    }
    _indexType = indexType;
    _indexBuffer = [indexBuffer retain];
    _indexCount = submesh.indexCount;
    _mesh = mesh;
    _name = [submesh.name copy];
  }
  return self;
}

- (void)dealloc {
  [_indexBuffer release];
  [_name release];
  [super dealloc];
}

- (NSString *)name {
  return _name;
}

- (void)setName:(NSString *)name {
  NSString *old = _name;
  _name = [name copy];
  [old release];
}

@end

@implementation MTKMesh {
  NSArray<MTKMeshBuffer *> *_vertexBuffers;
  MDLVertexDescriptor *_vertexDescriptor;
  NSArray<MTKSubmesh *> *_submeshes;
  NSUInteger _vertexCount;
  NSString *_name;
}

@synthesize vertexBuffers = _vertexBuffers, vertexDescriptor = _vertexDescriptor, submeshes = _submeshes, vertexCount = _vertexCount;

// a buffer the mesh already keeps in Metal is shared, any other is copied into one
static MTKMeshBuffer *metal_buffer(id<MDLMeshBuffer> source, MTKMeshBufferAllocator *allocator, MDLMeshBufferType type) {
  if ([(id)source isKindOfClass:[MTKMeshBuffer class]] && [((MTKMeshBuffer *)source).buffer device] == allocator.device)
    return [(MTKMeshBuffer *)source retain];

  MDLMeshBufferMap *map = [source map];
  NSData *data = [NSData dataWithBytes:[map bytes] length:[source length]];
  return (MTKMeshBuffer *)[allocator newBufferWithData:data type:type];
}

- (instancetype)initWithMesh:(MDLMesh *)mesh device:(id<MTLDevice>)device error:(NSError **)error {
  if (!(self = [super init]))
    return nil;

  MTKMeshBufferAllocator *allocator = [[[MTKMeshBufferAllocator alloc] initWithDevice:device] autorelease];
  NSMutableArray *vertexBuffers = [NSMutableArray array];
  NSMutableArray *submeshes = [NSMutableArray array];

  for (id<MDLMeshBuffer> source in mesh.vertexBuffers) {
    MTKMeshBuffer *buffer = metal_buffer(source, allocator, MDLMeshBufferTypeVertex);
    if (!buffer) {
      if (error)
        *error = model_error(@"Could not create a Metal vertex buffer");
      [self release];
      return nil;
    }
    [vertexBuffers addObject:buffer];
    [buffer release];
  }

  for (MDLSubmesh *submesh in mesh.submeshes) {
    // metal indexes with 16 or 32 bits, so 8-bit indices are widened
    id<MDLMeshBuffer> indices = submesh.indexType == MDLIndexBitDepthUInt8 ? [submesh indexBufferAsIndexType:MDLIndexBitDepthUInt16]
                                                                          : submesh.indexBuffer;
    MTLIndexType indexType = submesh.indexType == MDLIndexBitDepthUInt32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    MTKMeshBuffer *buffer = metal_buffer(indices, allocator, MDLMeshBufferTypeIndex);
    if (!buffer) {
      if (error)
        *error = model_error(@"Could not create a Metal index buffer");
      [self release];
      return nil;
    }
    MTKSubmesh *converted = [[MTKSubmesh alloc] initWithSubmesh:submesh indexBuffer:buffer indexType:indexType mesh:self];
    [submeshes addObject:converted];
    [converted release];
    [buffer release];
  }

  _vertexBuffers = [vertexBuffers copy];
  _submeshes = [submeshes copy];
  _vertexDescriptor = [mesh.vertexDescriptor retain];
  _vertexCount = mesh.vertexCount;
  _name = [mesh.name copy];
  return self;
}

static void collect_meshes(MDLObject *object, NSMutableArray *into) {
  if ([object isKindOfClass:[MDLMesh class]])
    [into addObject:object];
  for (MDLObject *child in object.children)
    collect_meshes(child, into);
}

+ (NSArray<MTKMesh *> *)newMeshesFromAsset:(MDLAsset *)asset device:(id<MTLDevice>)device
                              sourceMeshes:(NSArray<MDLMesh *> **)sourceMeshes error:(NSError **)error {
  NSMutableArray *sources = [NSMutableArray array];
  NSMutableArray *meshes = [[NSMutableArray alloc] init];

  for (MDLObject *object in asset)
    collect_meshes(object, sources);
  for (MDLMesh *source in sources) {
    MTKMesh *mesh = [[MTKMesh alloc] initWithMesh:source device:device error:error];
    if (!mesh) {
      [meshes release];
      return nil;
    }
    [meshes addObject:mesh];
    [mesh release];
  }
  if (sourceMeshes)
    *sourceMeshes = sources;
  return meshes;
}

- (void)dealloc {
  [_vertexBuffers release];
  [_vertexDescriptor release];
  [_submeshes release];
  [_name release];
  [super dealloc];
}

- (NSString *)name {
  return _name;
}

- (void)setName:(NSString *)name {
  NSString *old = _name;
  _name = [name copy];
  [old release];
}

@end

// vertex formats both frameworks know

static const struct { MTLVertexFormat metal; MDLVertexFormat modelIO; } formats[] = {
  { MTLVertexFormatUChar, MDLVertexFormatUChar }, { MTLVertexFormatUChar2, MDLVertexFormatUChar2 },
  { MTLVertexFormatUChar3, MDLVertexFormatUChar3 }, { MTLVertexFormatUChar4, MDLVertexFormatUChar4 },
  { MTLVertexFormatChar, MDLVertexFormatChar }, { MTLVertexFormatChar2, MDLVertexFormatChar2 },
  { MTLVertexFormatChar3, MDLVertexFormatChar3 }, { MTLVertexFormatChar4, MDLVertexFormatChar4 },
  { MTLVertexFormatUCharNormalized, MDLVertexFormatUCharNormalized }, { MTLVertexFormatUChar2Normalized, MDLVertexFormatUChar2Normalized },
  { MTLVertexFormatUChar3Normalized, MDLVertexFormatUChar3Normalized }, { MTLVertexFormatUChar4Normalized, MDLVertexFormatUChar4Normalized },
  { MTLVertexFormatCharNormalized, MDLVertexFormatCharNormalized }, { MTLVertexFormatChar2Normalized, MDLVertexFormatChar2Normalized },
  { MTLVertexFormatChar3Normalized, MDLVertexFormatChar3Normalized }, { MTLVertexFormatChar4Normalized, MDLVertexFormatChar4Normalized },
  { MTLVertexFormatUShort, MDLVertexFormatUShort }, { MTLVertexFormatUShort2, MDLVertexFormatUShort2 },
  { MTLVertexFormatUShort3, MDLVertexFormatUShort3 }, { MTLVertexFormatUShort4, MDLVertexFormatUShort4 },
  { MTLVertexFormatShort, MDLVertexFormatShort }, { MTLVertexFormatShort2, MDLVertexFormatShort2 },
  { MTLVertexFormatShort3, MDLVertexFormatShort3 }, { MTLVertexFormatShort4, MDLVertexFormatShort4 },
  { MTLVertexFormatUShortNormalized, MDLVertexFormatUShortNormalized }, { MTLVertexFormatUShort2Normalized, MDLVertexFormatUShort2Normalized },
  { MTLVertexFormatUShort3Normalized, MDLVertexFormatUShort3Normalized }, { MTLVertexFormatUShort4Normalized, MDLVertexFormatUShort4Normalized },
  { MTLVertexFormatShortNormalized, MDLVertexFormatShortNormalized }, { MTLVertexFormatShort2Normalized, MDLVertexFormatShort2Normalized },
  { MTLVertexFormatShort3Normalized, MDLVertexFormatShort3Normalized }, { MTLVertexFormatShort4Normalized, MDLVertexFormatShort4Normalized },
  { MTLVertexFormatHalf, MDLVertexFormatHalf }, { MTLVertexFormatHalf2, MDLVertexFormatHalf2 },
  { MTLVertexFormatHalf3, MDLVertexFormatHalf3 }, { MTLVertexFormatHalf4, MDLVertexFormatHalf4 },
  { MTLVertexFormatFloat, MDLVertexFormatFloat }, { MTLVertexFormatFloat2, MDLVertexFormatFloat2 },
  { MTLVertexFormatFloat3, MDLVertexFormatFloat3 }, { MTLVertexFormatFloat4, MDLVertexFormatFloat4 },
  { MTLVertexFormatInt, MDLVertexFormatInt }, { MTLVertexFormatInt2, MDLVertexFormatInt2 },
  { MTLVertexFormatInt3, MDLVertexFormatInt3 }, { MTLVertexFormatInt4, MDLVertexFormatInt4 },
  { MTLVertexFormatUInt, MDLVertexFormatUInt }, { MTLVertexFormatUInt2, MDLVertexFormatUInt2 },
  { MTLVertexFormatUInt3, MDLVertexFormatUInt3 }, { MTLVertexFormatUInt4, MDLVertexFormatUInt4 },
  { MTLVertexFormatInt1010102Normalized, MDLVertexFormatInt1010102Normalized },
  { MTLVertexFormatUInt1010102Normalized, MDLVertexFormatUInt1010102Normalized },
};

MDLVertexFormat MTKModelIOVertexFormatFromMetal(MTLVertexFormat vertexFormat) {
  for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
    if (formats[i].metal == vertexFormat)
      return formats[i].modelIO;
  }
  return MDLVertexFormatInvalid;
}

MTLVertexFormat MTKMetalVertexFormatFromModelIO(MDLVertexFormat vertexFormat) {
  for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
    if (formats[i].modelIO == vertexFormat)
      return formats[i].metal;
  }
  return MTLVertexFormatInvalid;
}

// all 31 slots, unnamed as ModelIO leaves them, for the caller to name
MDLVertexDescriptor *MTKModelIOVertexDescriptorFromMetalWithError(MTLVertexDescriptor *metalDescriptor, NSError **error) {
  MDLVertexDescriptor *descriptor = [[[MDLVertexDescriptor alloc] init] autorelease];

  for (NSUInteger i = 0; i < [descriptor.attributes count]; i++) {
    MTLVertexAttributeDescriptor *source = metalDescriptor.attributes[i];
    MDLVertexAttribute *attribute = descriptor.attributes[i];

    attribute.format = MTKModelIOVertexFormatFromMetal(source.format);
    attribute.offset = source.offset;
    attribute.bufferIndex = source.bufferIndex;
  }
  for (NSUInteger i = 0; i < [descriptor.layouts count]; i++)
    descriptor.layouts[i].stride = metalDescriptor.layouts[i].stride;
  return descriptor;
}

MDLVertexDescriptor *MTKModelIOVertexDescriptorFromMetal(MTLVertexDescriptor *metalDescriptor) {
  return MTKModelIOVertexDescriptorFromMetalWithError(metalDescriptor, NULL);
}

MTLVertexDescriptor *MTKMetalVertexDescriptorFromModelIOWithError(MDLVertexDescriptor *modelIODescriptor, NSError **error) {
  MTLVertexDescriptor *descriptor = [MTLVertexDescriptor vertexDescriptor];

  for (NSUInteger i = 0; i < [modelIODescriptor.attributes count] && i < 31; i++) {
    MDLVertexAttribute *source = modelIODescriptor.attributes[i];
    if (source.format == MDLVertexFormatInvalid)
      continue;
    descriptor.attributes[i].format = MTKMetalVertexFormatFromModelIO(source.format);
    descriptor.attributes[i].offset = source.offset;
    descriptor.attributes[i].bufferIndex = source.bufferIndex;
  }
  for (NSUInteger i = 0; i < [modelIODescriptor.layouts count] && i < 31; i++) {
    NSUInteger stride = modelIODescriptor.layouts[i].stride;
    if (stride == 0)
      continue;
    descriptor.layouts[i].stride = stride;
    descriptor.layouts[i].stepFunction = MTLVertexStepFunctionPerVertex;
    descriptor.layouts[i].stepRate = 1;
  }
  return descriptor;
}

MTLVertexDescriptor *MTKMetalVertexDescriptorFromModelIO(MDLVertexDescriptor *modelIODescriptor) {
  return MTKMetalVertexDescriptorFromModelIOWithError(modelIODescriptor, NULL);
}

// the loader's own helpers in MTKTextureLoader.mm
@interface MTKTextureLoader (Helpers)
- (MTLTextureDescriptor *)descriptorWithFormat:(MTLPixelFormat)format width:(NSUInteger)w height:(NSUInteger)h
                                        levels:(NSUInteger)levels options:(NSDictionary *)options;
- (void)generateMipmaps:(id<MTLTexture>)texture;
@end

@implementation MTKTextureLoader (ModelIO)

// 8-bit RGBA texels, a cube's faces stacked top to bottom in slice order
- (id<MTLTexture>)newTextureWithMDLTexture:(MDLTexture *)texture options:(NSDictionary *)options error:(NSError **)error {
  NSData *texels = [texture texelDataWithTopLeftOrigin];
  NSUInteger w = texture.dimensions.x, h = texture.dimensions.y;
  NSNumber *srgb = [options objectForKey:MTKTextureLoaderOptionSRGB];
  BOOL generate = [[options objectForKey:MTKTextureLoaderOptionGenerateMipmaps] boolValue];

  if (!texels || texture.channelCount != 4 || texture.channelEncoding != MDLTextureChannelEncodingUInt8 || !w || !h) {
    if (error)
      *error = model_error(@"Only 8-bit RGBA ModelIO textures can be loaded");
    return nil;
  }
  MTLPixelFormat format = (srgb == nil || [srgb boolValue]) ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm;
  NSUInteger levels = 1;
  if (generate) {
    for (NSUInteger s = MAX(w, h); s > 1; s >>= 1)
      levels++;
  }
  MTLTextureDescriptor *descriptor = [self descriptorWithFormat:format width:w height:h levels:levels options:options];
  if (texture.isCube)
    descriptor.textureType = MTLTextureTypeCube;

  id<MTLTexture> result = [self.device newTextureWithDescriptor:descriptor];
  if (!result) {
    if (error)
      *error = model_error(@"Could not create a texture for the ModelIO texture");
    return nil;
  }
  NSUInteger faceBytes = texture.rowStride * h;
  for (NSUInteger slice = 0; slice < (texture.isCube ? 6 : 1); slice++) {
    [result replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 slice:slice
                withBytes:(const uint8_t *)[texels bytes] + slice * faceBytes bytesPerRow:texture.rowStride bytesPerImage:faceBytes];
  }
  if (levels > 1)
    [self generateMipmaps:result];
  return result;
}

@end

// the size MTKView sizes its drawable to: the view's bounds, as PDMetalKit.mm sizes the layer in points
@implementation MTKView (PDPreferredDrawableSize)

- (CGSize)preferredDrawableSize {
  NSSize size = [self bounds].size;

  return CGSizeMake(size.width, size.height);
}

@end
