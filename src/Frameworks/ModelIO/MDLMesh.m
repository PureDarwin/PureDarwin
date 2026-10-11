// MDLMesh.m - mesh buffers, vertex descriptors and meshes
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import "ModelIO.h"
#import "MDLPrivate.h"
#include <math.h>
#include <string.h>

NSString * const MDLVertexAttributeAnisotropy = @"anisotropy";
NSString * const MDLVertexAttributeBinormal = @"binormal";
NSString * const MDLVertexAttributeBitangent = @"bitangent";
NSString * const MDLVertexAttributeColor = @"color";
NSString * const MDLVertexAttributeEdgeCrease = @"edgeCrease";
NSString * const MDLVertexAttributeJointIndices = @"jointIndices";
NSString * const MDLVertexAttributeJointWeights = @"jointWeights";
NSString * const MDLVertexAttributeNormal = @"normal";
NSString * const MDLVertexAttributeOcclusionValue = @"occlusionValue";
NSString * const MDLVertexAttributePosition = @"position";
NSString * const MDLVertexAttributeShadingBasisU = @"shadingBasisU";
NSString * const MDLVertexAttributeShadingBasisV = @"shadingBasisV";
NSString * const MDLVertexAttributeSubdivisionStencil = @"subdivisionStencil";
NSString * const MDLVertexAttributeTangent = @"tangent";
NSString * const MDLVertexAttributeTextureCoordinate = @"textureCoordinate";

// ModelIO's descriptors always hold this many attribute and layout slots
#define MDL_SLOTS 31

// vertex formats

NSUInteger MDLVertexFormatComponents(MDLVertexFormat format)
{
    return format & 0xFF;
}

NSUInteger MDLVertexFormatSize(MDLVertexFormat format)
{
    if (format & MDLVertexFormatPackedBit)
        return 4;

    NSUInteger n = format & 0xFF;
    switch (format & 0xF0000) {
    case MDLVertexFormatUCharBits: case MDLVertexFormatCharBits:
    case MDLVertexFormatUCharNormalizedBits: case MDLVertexFormatCharNormalizedBits:
        return n;
    case MDLVertexFormatUShortBits: case MDLVertexFormatShortBits:
    case MDLVertexFormatUShortNormalizedBits: case MDLVertexFormatShortNormalizedBits:
    case MDLVertexFormatHalfBits:
        return 2 * n;
    case MDLVertexFormatUIntBits: case MDLVertexFormatIntBits: case MDLVertexFormatFloatBits:
        return 4 * n;
    }
    return 0;
}

static float halfToFloat(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16, exponent = (h >> 10) & 0x1F, mantissa = h & 0x3FF, bits;

    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exponent = 127 - 15 + 1;
            while (!(mantissa & 0x400)) {
                mantissa <<= 1;
                exponent--;
            }
            bits = sign | (exponent << 23) | ((mantissa & 0x3FF) << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7F800000 | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

static uint16_t floatToHalf(float f)
{
    uint32_t bits;
    memcpy(&bits, &f, 4);
    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exponent = (int32_t)((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFF;

    if (((bits >> 23) & 0xFF) == 0xFF)
        return (uint16_t)(sign | 0x7C00 | (mantissa ? 0x200 : 0));
    if (exponent >= 31)
        return (uint16_t)(sign | 0x7C00);
    if (exponent <= 0) {
        if (exponent < -10)
            return (uint16_t)sign;
        mantissa |= 0x800000;
        uint32_t shift = (uint32_t)(14 - exponent);
        uint32_t half = mantissa >> shift;
        if ((mantissa >> (shift - 1)) & 1)
            half++;
        return (uint16_t)(sign | half);
    }
    uint32_t half = sign | ((uint32_t)exponent << 10) | (mantissa >> 13);
    if (mantissa & 0x1000)
        half++;
    return (uint16_t)half;
}

// reads one element as up to four floats, the missing components left as they were
void MDLReadVertexElement(const void *source, MDLVertexFormat format, float out[4])
{
    NSUInteger n = format & 0xFF;
    const uint8_t *p = source;

    if (format & MDLVertexFormatPackedBit) {
        uint32_t v;
        memcpy(&v, p, 4);
        BOOL isSigned = (format & 0xF0000) == MDLVertexFormatIntBits;
        for (int i = 0; i < 3; i++) {
            int32_t c = (int32_t)((v >> (10 * i)) & 0x3FF);
            if (isSigned && (c & 0x200))
                c -= 0x400;
            out[i] = isSigned ? fmaxf((float)c / 511.0f, -1.0f) : (float)c / 1023.0f;
        }
        int32_t w = (int32_t)(v >> 30);
        out[3] = isSigned ? fmaxf((float)((w & 2) ? w - 4 : w), -1.0f) : (float)w / 3.0f;
        return;
    }
    for (NSUInteger i = 0; i < n && i < 4; i++) {
        switch (format & 0xF0000) {
        case MDLVertexFormatUCharBits: out[i] = p[i]; break;
        case MDLVertexFormatCharBits: out[i] = (int8_t)p[i]; break;
        case MDLVertexFormatUCharNormalizedBits: out[i] = p[i] / 255.0f; break;
        case MDLVertexFormatCharNormalizedBits: out[i] = fmaxf((int8_t)p[i] / 127.0f, -1.0f); break;
        case MDLVertexFormatUShortBits: { uint16_t x; memcpy(&x, p + 2 * i, 2); out[i] = x; break; }
        case MDLVertexFormatShortBits: { int16_t x; memcpy(&x, p + 2 * i, 2); out[i] = x; break; }
        case MDLVertexFormatUShortNormalizedBits: { uint16_t x; memcpy(&x, p + 2 * i, 2); out[i] = x / 65535.0f; break; }
        case MDLVertexFormatShortNormalizedBits: { int16_t x; memcpy(&x, p + 2 * i, 2); out[i] = fmaxf(x / 32767.0f, -1.0f); break; }
        case MDLVertexFormatHalfBits: { uint16_t x; memcpy(&x, p + 2 * i, 2); out[i] = halfToFloat(x); break; }
        case MDLVertexFormatUIntBits: { uint32_t x; memcpy(&x, p + 4 * i, 4); out[i] = (float)x; break; }
        case MDLVertexFormatIntBits: { int32_t x; memcpy(&x, p + 4 * i, 4); out[i] = (float)x; break; }
        case MDLVertexFormatFloatBits: memcpy(&out[i], p + 4 * i, 4); break;
        }
    }
}

static float clampUnit(float v, float lo)
{
    return v < lo ? lo : (v > 1.0f ? 1.0f : v);
}

void MDLWriteVertexElement(void *destination, MDLVertexFormat format, const float in[4])
{
    NSUInteger n = format & 0xFF;
    uint8_t *p = destination;

    if (format & MDLVertexFormatPackedBit) {
        BOOL isSigned = (format & 0xF0000) == MDLVertexFormatIntBits;
        uint32_t v = 0;
        for (int i = 0; i < 3; i++) {
            int32_t c = isSigned ? (int32_t)lroundf(clampUnit(in[i], -1.0f) * 511.0f) : (int32_t)lroundf(clampUnit(in[i], 0.0f) * 1023.0f);
            v |= ((uint32_t)c & 0x3FF) << (10 * i);
        }
        int32_t w = isSigned ? (int32_t)lroundf(clampUnit(in[3], -1.0f)) : (int32_t)lroundf(clampUnit(in[3], 0.0f) * 3.0f);
        v |= ((uint32_t)w & 3) << 30;
        memcpy(p, &v, 4);
        return;
    }
    for (NSUInteger i = 0; i < n && i < 4; i++) {
        float x = in[i];
        switch (format & 0xF0000) {
        case MDLVertexFormatUCharBits: p[i] = (uint8_t)x; break;
        case MDLVertexFormatCharBits: p[i] = (uint8_t)(int8_t)x; break;
        case MDLVertexFormatUCharNormalizedBits: p[i] = (uint8_t)lroundf(clampUnit(x, 0.0f) * 255.0f); break;
        case MDLVertexFormatCharNormalizedBits: p[i] = (uint8_t)(int8_t)lroundf(clampUnit(x, -1.0f) * 127.0f); break;
        case MDLVertexFormatUShortBits: { uint16_t v = (uint16_t)x; memcpy(p + 2 * i, &v, 2); break; }
        case MDLVertexFormatShortBits: { int16_t v = (int16_t)x; memcpy(p + 2 * i, &v, 2); break; }
        case MDLVertexFormatUShortNormalizedBits: { uint16_t v = (uint16_t)lroundf(clampUnit(x, 0.0f) * 65535.0f); memcpy(p + 2 * i, &v, 2); break; }
        case MDLVertexFormatShortNormalizedBits: { int16_t v = (int16_t)lroundf(clampUnit(x, -1.0f) * 32767.0f); memcpy(p + 2 * i, &v, 2); break; }
        case MDLVertexFormatHalfBits: { uint16_t v = floatToHalf(x); memcpy(p + 2 * i, &v, 2); break; }
        case MDLVertexFormatUIntBits: { uint32_t v = (uint32_t)x; memcpy(p + 4 * i, &v, 4); break; }
        case MDLVertexFormatIntBits: { int32_t v = (int32_t)x; memcpy(p + 4 * i, &v, 4); break; }
        case MDLVertexFormatFloatBits: memcpy(p + 4 * i, &x, 4); break;
        }
    }
}

// mesh buffers

@implementation MDLMeshBufferMap {
    void *_bytes;
    void (^_deallocator)(void);
}

- (instancetype)initWithBytes:(void *)bytes deallocator:(void (^)(void))deallocator {
    if ((self = [super init]) != nil) {
        _bytes = bytes;
        _deallocator = [deallocator copy];
    }
    return self;
}

- (void)dealloc {
    if (_deallocator != nil)
        _deallocator();
    [_deallocator release];
    [super dealloc];
}

- (void *)bytes {
    return _bytes;
}

@end

@implementation MDLMeshBufferZoneDefault {
    NSUInteger _capacity;
    id <MDLMeshBufferAllocator> _allocator;
}

- (instancetype)initWithCapacity:(NSUInteger)capacity allocator:(id <MDLMeshBufferAllocator>)allocator {
    if ((self = [super init]) != nil) {
        _capacity = capacity;
        _allocator = [allocator retain];
    }
    return self;
}

- (void)dealloc {
    [_allocator release];
    [super dealloc];
}

- (NSUInteger)capacity {
    return _capacity;
}

- (id <MDLMeshBufferAllocator>)allocator {
    return _allocator;
}

@end

@implementation MDLMeshBufferData {
    NSMutableData *_data;
    MDLMeshBufferType _type;
    id <MDLMeshBufferAllocator> _allocator;
}

- (instancetype)initWithType:(MDLMeshBufferType)type length:(NSUInteger)length {
    if ((self = [super init]) != nil) {
        _type = type;
        _data = [[NSMutableData alloc] initWithLength:length];
    }
    return self;
}

- (instancetype)initWithType:(MDLMeshBufferType)type data:(NSData *)data {
    if ((self = [super init]) != nil) {
        _type = type;
        _data = data != nil ? [data mutableCopy] : [[NSMutableData alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_data release];
    [_allocator release];
    [super dealloc];
}

- (void)_setAllocator:(id <MDLMeshBufferAllocator>)allocator {
    [_allocator release];
    _allocator = [allocator retain];
}

- (id)copyWithZone:(NSZone *)zone {
    MDLMeshBufferData *copy = [[MDLMeshBufferData alloc] initWithType:_type data:_data];
    [copy _setAllocator:_allocator];
    return copy;
}

- (NSData *)data {
    return _data;
}

- (void)fillData:(NSData *)data offset:(NSUInteger)offset {
    NSUInteger length = [data length];

    if (offset >= [_data length])
        return;
    if (length > [_data length] - offset)
        length = [_data length] - offset;
    memcpy((uint8_t *)[_data mutableBytes] + offset, [data bytes], length);
}

- (MDLMeshBufferMap *)map {
    NSMutableData *keep = [_data retain];
    return [[[MDLMeshBufferMap alloc] initWithBytes:[_data mutableBytes] deallocator:^{ [keep release]; }] autorelease];
}

- (NSUInteger)length {
    return [_data length];
}

- (id <MDLMeshBufferAllocator>)allocator {
    return _allocator;
}

- (id <MDLMeshBufferZone>)zone {
    return nil;
}

- (MDLMeshBufferType)type {
    return _type;
}

@end

@implementation MDLMeshBufferDataAllocator

- (id <MDLMeshBufferZone>)newZone:(NSUInteger)capacity {
    return [[MDLMeshBufferZoneDefault alloc] initWithCapacity:capacity allocator:self];
}

- (id <MDLMeshBufferZone>)newZoneForBuffersWithSize:(NSArray<NSNumber *> *)sizes andType:(NSArray<NSNumber *> *)types {
    NSUInteger total = 0;

    for (NSNumber *size in sizes)
        total += [size unsignedIntegerValue];
    return [self newZone:total];
}

- (id <MDLMeshBuffer>)newBuffer:(NSUInteger)length type:(MDLMeshBufferType)type {
    MDLMeshBufferData *buffer = [[MDLMeshBufferData alloc] initWithType:type length:length];
    [buffer _setAllocator:self];
    return buffer;
}

- (id <MDLMeshBuffer>)newBufferWithData:(NSData *)data type:(MDLMeshBufferType)type {
    MDLMeshBufferData *buffer = [[MDLMeshBufferData alloc] initWithType:type data:data];
    [buffer _setAllocator:self];
    return buffer;
}

- (id <MDLMeshBuffer>)newBufferFromZone:(id <MDLMeshBufferZone>)zone length:(NSUInteger)length type:(MDLMeshBufferType)type {
    return [self newBuffer:length type:type];
}

- (id <MDLMeshBuffer>)newBufferFromZone:(id <MDLMeshBufferZone>)zone data:(NSData *)data type:(MDLMeshBufferType)type {
    return [self newBufferWithData:data type:type];
}

@end

// vertex descriptors

@implementation MDLVertexAttribute {
    NSString *_name;
    MDLVertexFormat _format;
    NSUInteger _offset, _bufferIndex;
    NSTimeInterval _time;
    vector_float4 _initializationValue;
}

- (instancetype)init {
    return [self initWithName:@"" format:MDLVertexFormatInvalid offset:0 bufferIndex:0];
}

- (instancetype)initWithName:(NSString *)name format:(MDLVertexFormat)format offset:(NSUInteger)offset bufferIndex:(NSUInteger)bufferIndex {
    if ((self = [super init]) != nil) {
        _name = [name copy];
        _format = format;
        _offset = offset;
        _bufferIndex = bufferIndex;
        _initializationValue = (vector_float4){ 0, 0, 0, 1 };
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    MDLVertexAttribute *copy = [[MDLVertexAttribute alloc] initWithName:_name format:_format offset:_offset bufferIndex:_bufferIndex];
    copy->_time = _time;
    copy->_initializationValue = _initializationValue;
    return copy;
}

- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { NSString *old = _name; _name = [name copy]; [old release]; }
- (MDLVertexFormat)format { return _format; }
- (void)setFormat:(MDLVertexFormat)format { _format = format; }
- (NSUInteger)offset { return _offset; }
- (void)setOffset:(NSUInteger)offset { _offset = offset; }
- (NSUInteger)bufferIndex { return _bufferIndex; }
- (void)setBufferIndex:(NSUInteger)index { _bufferIndex = index; }
- (NSTimeInterval)time { return _time; }
- (void)setTime:(NSTimeInterval)time { _time = time; }
- (vector_float4)initializationValue { return _initializationValue; }
- (void)setInitializationValue:(vector_float4)value { _initializationValue = value; }

- (NSString *)description {
    return [NSString stringWithFormat:@"<MDLVertexAttribute %@ format 0x%lx offset %lu buffer %lu>", _name,
            (unsigned long)_format, (unsigned long)_offset, (unsigned long)_bufferIndex];
}

@end

@implementation MDLVertexBufferLayout {
    NSUInteger _stride;
}

- (instancetype)initWithStride:(NSUInteger)stride {
    if ((self = [super init]) != nil)
        _stride = stride;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return [[MDLVertexBufferLayout alloc] initWithStride:_stride];
}

- (NSUInteger)stride { return _stride; }
- (void)setStride:(NSUInteger)stride { _stride = stride; }

@end

@implementation MDLVertexDescriptor {
    NSMutableArray *_attributes, *_layouts;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _attributes = [[NSMutableArray alloc] init];
        _layouts = [[NSMutableArray alloc] init];
        [self reset];
    }
    return self;
}

- (instancetype)initWithVertexDescriptor:(MDLVertexDescriptor *)vertexDescriptor {
    if ((self = [super init]) != nil) {
        _attributes = [[NSMutableArray alloc] init];
        _layouts = [[NSMutableArray alloc] init];
        for (MDLVertexAttribute *attribute in vertexDescriptor.attributes)
            [_attributes addObject:[[attribute copy] autorelease]];
        for (MDLVertexBufferLayout *layout in vertexDescriptor.layouts)
            [_layouts addObject:[[layout copy] autorelease]];
    }
    return self;
}

- (void)dealloc {
    [_attributes release];
    [_layouts release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [[MDLVertexDescriptor alloc] initWithVertexDescriptor:self];
}

- (NSMutableArray *)attributes { return _attributes; }
- (void)setAttributes:(NSMutableArray *)attributes { NSMutableArray *old = _attributes; _attributes = [attributes mutableCopy]; [old release]; }
- (NSMutableArray *)layouts { return _layouts; }
- (void)setLayouts:(NSMutableArray *)layouts { NSMutableArray *old = _layouts; _layouts = [layouts mutableCopy]; [old release]; }

- (void)reset {
    [_attributes removeAllObjects];
    [_layouts removeAllObjects];
    for (int i = 0; i < MDL_SLOTS; i++) {
        [_attributes addObject:[[[MDLVertexAttribute alloc] init] autorelease]];
        [_layouts addObject:[[[MDLVertexBufferLayout alloc] initWithStride:0] autorelease]];
    }
}

- (MDLVertexAttribute *)attributeNamed:(NSString *)name {
    for (MDLVertexAttribute *attribute in _attributes) {
        if (attribute.format != MDLVertexFormatInvalid && [attribute.name isEqualToString:name])
            return attribute;
    }
    return nil;
}

- (void)addOrReplaceAttribute:(MDLVertexAttribute *)attribute {
    for (NSUInteger i = 0; i < [_attributes count]; i++) {
        MDLVertexAttribute *existing = [_attributes objectAtIndex:i];
        if ([existing.name isEqualToString:attribute.name] || existing.format == MDLVertexFormatInvalid) {
            [_attributes replaceObjectAtIndex:i withObject:[[attribute copy] autorelease]];
            return;
        }
    }
    [_attributes addObject:[[attribute copy] autorelease]];
}

- (void)removeAttributeNamed:(NSString *)name {
    for (NSUInteger i = 0; i < [_attributes count]; i++) {
        if ([[[_attributes objectAtIndex:i] name] isEqualToString:name])
            [_attributes replaceObjectAtIndex:i withObject:[[[MDLVertexAttribute alloc] init] autorelease]];
    }
}

// each buffer's stride becomes the end of its furthest attribute
- (void)setPackedStrides {
    for (NSUInteger b = 0; b < [_layouts count]; b++) {
        NSUInteger stride = 0;
        for (MDLVertexAttribute *attribute in _attributes) {
            if (attribute.format != MDLVertexFormatInvalid && attribute.bufferIndex == b)
                stride = MAX(stride, attribute.offset + MDLVertexFormatSize(attribute.format));
        }
        [[_layouts objectAtIndex:b] setStride:stride];
    }
}

// attributes sharing a buffer are laid out one after another, in descriptor order
- (void)setPackedOffsets {
    NSUInteger offsets[64] = { 0 };

    for (MDLVertexAttribute *attribute in _attributes) {
        if (attribute.format == MDLVertexFormatInvalid || attribute.bufferIndex >= 64)
            continue;
        attribute.offset = offsets[attribute.bufferIndex];
        offsets[attribute.bufferIndex] += MDLVertexFormatSize(attribute.format);
    }
}

@end

@implementation MDLVertexAttributeData {
    MDLMeshBufferMap *_map;
    void *_dataStart;
    NSUInteger _stride, _bufferSize;
    MDLVertexFormat _format;
}

- (void)dealloc {
    [_map release];
    [super dealloc];
}

- (MDLMeshBufferMap *)map { return _map; }
- (void)setMap:(MDLMeshBufferMap *)map { [map retain]; [_map release]; _map = map; }
- (void *)dataStart { return _dataStart; }
- (void)setDataStart:(void *)start { _dataStart = start; }
- (NSUInteger)stride { return _stride; }
- (void)setStride:(NSUInteger)stride { _stride = stride; }
- (MDLVertexFormat)format { return _format; }
- (void)setFormat:(MDLVertexFormat)format { _format = format; }
- (NSUInteger)bufferSize { return _bufferSize; }
- (void)setBufferSize:(NSUInteger)size { _bufferSize = size; }

@end

// submeshes and materials

@implementation MDLMaterial {
    NSString *_name;
}

- (instancetype)initWithName:(NSString *)name scatteringFunction:(id)scatteringFunction {
    if ((self = [super init]) != nil)
        _name = [name copy];
    return self;
}

- (void)dealloc {
    [_name release];
    [super dealloc];
}

- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { NSString *old = _name; _name = [name copy]; [old release]; }
- (NSUInteger)count { return 0; }

@end

@implementation MDLSubmesh {
    NSString *_name;
    id <MDLMeshBuffer> _indexBuffer;
    NSUInteger _indexCount;
    MDLIndexBitDepth _indexType;
    MDLGeometryType _geometryType;
    MDLMaterial *_material;
}

- (instancetype)initWithName:(NSString *)name indexBuffer:(id <MDLMeshBuffer>)indexBuffer indexCount:(NSUInteger)indexCount
                   indexType:(MDLIndexBitDepth)indexType geometryType:(MDLGeometryType)geometryType material:(MDLMaterial *)material {
    if ((self = [super init]) != nil) {
        _name = [name copy];
        _indexBuffer = [indexBuffer retain];
        _indexCount = indexCount;
        _indexType = indexType;
        _geometryType = geometryType;
        _material = [material retain];
    }
    return self;
}

- (instancetype)initWithIndexBuffer:(id <MDLMeshBuffer>)indexBuffer indexCount:(NSUInteger)indexCount
                          indexType:(MDLIndexBitDepth)indexType geometryType:(MDLGeometryType)geometryType material:(MDLMaterial *)material {
    return [self initWithName:@"" indexBuffer:indexBuffer indexCount:indexCount indexType:indexType geometryType:geometryType material:material];
}

- (void)dealloc {
    [_name release];
    [_indexBuffer release];
    [_material release];
    [super dealloc];
}

- (id <MDLMeshBuffer>)indexBuffer { return _indexBuffer; }
- (NSUInteger)indexCount { return _indexCount; }
- (MDLIndexBitDepth)indexType { return _indexType; }
- (MDLGeometryType)geometryType { return _geometryType; }
- (MDLMaterial *)material { return _material; }
- (void)setMaterial:(MDLMaterial *)material { [material retain]; [_material release]; _material = material; }
- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { NSString *old = _name; _name = [name copy]; [old release]; }

- (id <MDLMeshBuffer>)indexBufferAsIndexType:(MDLIndexBitDepth)indexType {
    if (indexType == _indexType)
        return _indexBuffer;

    MDLMeshBufferMap *map = [_indexBuffer map];
    NSMutableData *data = [NSMutableData dataWithLength:_indexCount * (indexType / 8)];
    for (NSUInteger i = 0; i < _indexCount; i++) {
        uint32_t index = MDLReadIndex([map bytes], _indexType, i);
        switch (indexType) {
        case MDLIndexBitDepthUInt8: ((uint8_t *)[data mutableBytes])[i] = (uint8_t)index; break;
        case MDLIndexBitDepthUInt16: ((uint16_t *)[data mutableBytes])[i] = (uint16_t)index; break;
        default: ((uint32_t *)[data mutableBytes])[i] = index; break;
        }
    }
    return [[[_indexBuffer allocator] newBufferWithData:data type:MDLMeshBufferTypeIndex] autorelease];
}

@end

uint32_t MDLReadIndex(const void *indices, MDLIndexBitDepth type, NSUInteger i)
{
    switch (type) {
    case MDLIndexBitDepthUInt8: return ((const uint8_t *)indices)[i];
    case MDLIndexBitDepthUInt16: return ((const uint16_t *)indices)[i];
    default: return ((const uint32_t *)indices)[i];
    }
}

// meshes

@implementation MDLMesh {
    MDLVertexDescriptor *_vertexDescriptor;
    NSUInteger _vertexCount;
    NSArray *_vertexBuffers;
    NSMutableArray *_submeshes;
    id <MDLMeshBufferAllocator> _allocator;
}

- (instancetype)init {
    return [self initWithBufferAllocator:nil];
}

- (instancetype)initWithBufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator {
    if ((self = [super init]) != nil) {
        _allocator = bufferAllocator != nil ? [bufferAllocator retain] : [[MDLMeshBufferDataAllocator alloc] init];
        _vertexDescriptor = [[MDLVertexDescriptor alloc] init];
        _vertexBuffers = [[NSArray alloc] init];
        _submeshes = [[NSMutableArray alloc] init];
    }
    return self;
}

- (instancetype)initWithVertexBuffers:(NSArray<id <MDLMeshBuffer>> *)vertexBuffers vertexCount:(NSUInteger)vertexCount
                           descriptor:(MDLVertexDescriptor *)descriptor submeshes:(NSArray<MDLSubmesh *> *)submeshes {
    id <MDLMeshBufferAllocator> allocator = [[vertexBuffers firstObject] allocator];

    if ((self = [self initWithBufferAllocator:allocator]) != nil) {
        [_vertexDescriptor release];
        _vertexDescriptor = [descriptor copy];
        [_vertexBuffers release];
        _vertexBuffers = [vertexBuffers copy];
        _vertexCount = vertexCount;
        [_submeshes addObjectsFromArray:submeshes];
    }
    return self;
}

- (instancetype)initWithVertexBuffer:(id <MDLMeshBuffer>)vertexBuffer vertexCount:(NSUInteger)vertexCount
                          descriptor:(MDLVertexDescriptor *)descriptor submeshes:(NSArray<MDLSubmesh *> *)submeshes {
    return [self initWithVertexBuffers:@[ vertexBuffer ] vertexCount:vertexCount descriptor:descriptor submeshes:submeshes];
}

- (void)dealloc {
    [_vertexDescriptor release];
    [_vertexBuffers release];
    [_submeshes release];
    [_allocator release];
    [super dealloc];
}

- (id <MDLMeshBufferAllocator>)allocator { return _allocator; }
- (NSUInteger)vertexCount { return _vertexCount; }
- (void)setVertexCount:(NSUInteger)count { _vertexCount = count; }
- (NSArray *)vertexBuffers { return _vertexBuffers; }
- (void)setVertexBuffers:(NSArray *)buffers { NSArray *old = _vertexBuffers; _vertexBuffers = [buffers copy]; [old release]; }
- (NSMutableArray *)submeshes { return _submeshes; }
- (void)setSubmeshes:(NSMutableArray *)submeshes { NSMutableArray *old = _submeshes; _submeshes = [submeshes mutableCopy]; [old release]; }
- (MDLVertexDescriptor *)vertexDescriptor { return [[_vertexDescriptor copy] autorelease]; }

- (id <MDLMeshBuffer>)_bufferAtIndex:(NSUInteger)index {
    return index < [_vertexBuffers count] ? [_vertexBuffers objectAtIndex:index] : nil;
}

- (NSUInteger)_strideForBuffer:(NSUInteger)index {
    return index < [_vertexDescriptor.layouts count] ? [[_vertexDescriptor.layouts objectAtIndex:index] stride] : 0;
}

// every vertex's value of one attribute, as float4s with ModelIO's defaults for missing components
- (float *)_copyValuesOfAttribute:(MDLVertexAttribute *)attribute {
    float *values = malloc(sizeof(float) * 4 * (_vertexCount ? _vertexCount : 1));
    vector_float4 initial = attribute.initializationValue;
    id <MDLMeshBuffer> buffer = [self _bufferAtIndex:attribute.bufferIndex];
    NSUInteger stride = [self _strideForBuffer:attribute.bufferIndex];
    MDLMeshBufferMap *map = [buffer map];
    const uint8_t *bytes = [map bytes];

    for (NSUInteger v = 0; v < _vertexCount; v++) {
        float *out = values + 4 * v;
        out[0] = 0, out[1] = 0, out[2] = 0, out[3] = 1;
        if (bytes == NULL || attribute.offset + v * stride + MDLVertexFormatSize(attribute.format) > [buffer length]) {
            out[0] = initial.x, out[1] = initial.y, out[2] = initial.z, out[3] = initial.w;
            continue;
        }
        MDLReadVertexElement(bytes + v * stride + attribute.offset, attribute.format, out);
    }
    return values;
}

// lays the vertex data out again for the new descriptor, converting each attribute by name
- (void)setVertexDescriptor:(MDLVertexDescriptor *)descriptor {
    MDLVertexDescriptor *target = [[descriptor copy] autorelease];
    NSUInteger bufferCount = 0;

    for (MDLVertexAttribute *attribute in target.attributes) {
        if (attribute.format != MDLVertexFormatInvalid)
            bufferCount = MAX(bufferCount, attribute.bufferIndex + 1);
    }

    NSMutableArray *datas = [NSMutableArray array];
    for (NSUInteger b = 0; b < bufferCount; b++) {
        NSUInteger stride = b < [target.layouts count] ? [[target.layouts objectAtIndex:b] stride] : 0;
        [datas addObject:[NSMutableData dataWithLength:stride * _vertexCount]];
    }

    for (MDLVertexAttribute *attribute in target.attributes) {
        if (attribute.format == MDLVertexFormatInvalid)
            continue;
        NSUInteger b = attribute.bufferIndex;
        NSUInteger stride = [[target.layouts objectAtIndex:b] stride];
        uint8_t *out = [[datas objectAtIndex:b] mutableBytes];
        MDLVertexAttribute *source = [_vertexDescriptor attributeNamed:attribute.name];
        float *values = NULL;

        if (source != nil)
            values = [self _copyValuesOfAttribute:source];
        for (NSUInteger v = 0; v < _vertexCount; v++) {
            vector_float4 initial = attribute.initializationValue;
            float fallback[4] = { initial.x, initial.y, initial.z, initial.w };
            MDLWriteVertexElement(out + v * stride + attribute.offset, attribute.format, values ? values + 4 * v : fallback);
        }
        free(values);
    }

    NSMutableArray *buffers = [NSMutableArray array];
    for (NSMutableData *data in datas)
        [buffers addObject:[[_allocator newBufferWithData:data type:MDLMeshBufferTypeVertex] autorelease]];
    [self setVertexBuffers:buffers];
    [_vertexDescriptor release];
    _vertexDescriptor = [target retain];
}

- (MDLVertexAttributeData *)vertexAttributeDataForAttributeNamed:(NSString *)name {
    MDLVertexAttribute *attribute = [_vertexDescriptor attributeNamed:name];

    return attribute != nil ? [self vertexAttributeDataForAttributeNamed:name asFormat:attribute.format] : nil;
}

// the attribute's own storage when the format matches, otherwise a converted packed copy
- (MDLVertexAttributeData *)vertexAttributeDataForAttributeNamed:(NSString *)name asFormat:(MDLVertexFormat)format {
    MDLVertexAttribute *attribute = [_vertexDescriptor attributeNamed:name];
    if (attribute == nil)
        return nil;

    MDLVertexAttributeData *result = [[[MDLVertexAttributeData alloc] init] autorelease];
    id <MDLMeshBuffer> buffer = [self _bufferAtIndex:attribute.bufferIndex];
    if (buffer == nil)
        return nil;

    if (format == attribute.format) {
        MDLMeshBufferMap *map = [buffer map];
        result.map = map;
        result.dataStart = (uint8_t *)[map bytes] + attribute.offset;
        result.stride = [self _strideForBuffer:attribute.bufferIndex];
        result.format = format;
        result.bufferSize = [buffer length];
        return result;
    }

    NSUInteger size = MDLVertexFormatSize(format);
    float *values = [self _copyValuesOfAttribute:attribute];
    uint8_t *bytes = calloc(_vertexCount ? _vertexCount : 1, size);
    for (NSUInteger v = 0; v < _vertexCount; v++)
        MDLWriteVertexElement(bytes + v * size, format, values + 4 * v);
    free(values);

    result.map = [[[MDLMeshBufferMap alloc] initWithBytes:bytes deallocator:^{ free(bytes); }] autorelease];
    result.dataStart = bytes;
    result.stride = size;
    result.format = format;
    result.bufferSize = _vertexCount * size;
    return result;
}

// a new attribute in a buffer of its own, filled with its initialization value
- (void)addAttributeWithName:(NSString *)name format:(MDLVertexFormat)format {
    if ([_vertexDescriptor attributeNamed:name] != nil)
        return;

    NSUInteger bufferIndex = [_vertexBuffers count];
    MDLVertexDescriptor *descriptor = [[_vertexDescriptor copy] autorelease];
    MDLVertexAttribute *attribute = [[[MDLVertexAttribute alloc] initWithName:name format:format offset:0 bufferIndex:bufferIndex] autorelease];
    [descriptor addOrReplaceAttribute:attribute];
    while ([descriptor.layouts count] <= bufferIndex)
        [descriptor.layouts addObject:[[[MDLVertexBufferLayout alloc] initWithStride:0] autorelease]];
    [[descriptor.layouts objectAtIndex:bufferIndex] setStride:MDLVertexFormatSize(format)];

    NSMutableData *data = [NSMutableData dataWithLength:MDLVertexFormatSize(format) * _vertexCount];
    vector_float4 initial = attribute.initializationValue;
    float value[4] = { initial.x, initial.y, initial.z, initial.w };
    for (NSUInteger v = 0; v < _vertexCount; v++)
        MDLWriteVertexElement((uint8_t *)[data mutableBytes] + v * MDLVertexFormatSize(format), format, value);

    NSMutableArray *buffers = [[_vertexBuffers mutableCopy] autorelease];
    [buffers addObject:[[_allocator newBufferWithData:data type:MDLMeshBufferTypeVertex] autorelease]];
    [self setVertexBuffers:buffers];
    [_vertexDescriptor release];
    _vertexDescriptor = [descriptor retain];
}

// stores float4 values (x, y, z and w) into an attribute, adding it if the mesh lacks it
- (void)_storeValues:(const float *)values forAttribute:(NSString *)name format:(MDLVertexFormat)format {
    [self addAttributeWithName:name format:format];

    MDLVertexAttribute *attribute = [_vertexDescriptor attributeNamed:name];
    id <MDLMeshBuffer> buffer = [self _bufferAtIndex:attribute.bufferIndex];
    NSUInteger stride = [self _strideForBuffer:attribute.bufferIndex];
    NSUInteger size = MDLVertexFormatSize(attribute.format);
    NSMutableData *data = [NSMutableData dataWithLength:[buffer length]];
    MDLMeshBufferMap *map = [buffer map];

    memcpy([data mutableBytes], [map bytes], [buffer length]);
    for (NSUInteger v = 0; v < _vertexCount; v++) {
        if (v * stride + attribute.offset + size <= [data length])
            MDLWriteVertexElement((uint8_t *)[data mutableBytes] + v * stride + attribute.offset, attribute.format, values + 4 * v);
    }
    [buffer fillData:data offset:0];
}

// calls block with each triangle's three vertex indices, from every triangle submesh
- (void)_enumerateTriangles:(void (^)(uint32_t a, uint32_t b, uint32_t c))block {
    for (MDLSubmesh *submesh in _submeshes) {
        MDLMeshBufferMap *map = [submesh.indexBuffer map];
        const void *indices = [map bytes];
        NSUInteger count = submesh.indexCount;

        if (submesh.geometryType == MDLGeometryTypeTriangles) {
            for (NSUInteger i = 0; i + 2 < count; i += 3)
                block(MDLReadIndex(indices, submesh.indexType, i), MDLReadIndex(indices, submesh.indexType, i + 1),
                      MDLReadIndex(indices, submesh.indexType, i + 2));
        } else if (submesh.geometryType == MDLGeometryTypeTriangleStrips) {
            for (NSUInteger i = 0; i + 2 < count; i++) {
                uint32_t a = MDLReadIndex(indices, submesh.indexType, i), b = MDLReadIndex(indices, submesh.indexType, i + 1);
                uint32_t c = MDLReadIndex(indices, submesh.indexType, i + 2);
                if (i & 1)
                    block(b, a, c);
                else
                    block(a, b, c);
            }
        }
    }
}

// per-vertex sums of each triangle's texture-space tangent and bitangent directions
- (BOOL)_accumulateTangents:(float *)tangents bitangents:(float *)bitangents textureCoordinates:(NSString *)uvName {
    MDLVertexAttribute *positionAttribute = [_vertexDescriptor attributeNamed:MDLVertexAttributePosition];
    MDLVertexAttribute *uvAttribute = [_vertexDescriptor attributeNamed:uvName];
    if (positionAttribute == nil || uvAttribute == nil)
        return NO;

    float *positions = [self _copyValuesOfAttribute:positionAttribute];
    float *uvs = [self _copyValuesOfAttribute:uvAttribute];
    NSUInteger count = _vertexCount;

    memset(tangents, 0, sizeof(float) * 4 * count);
    memset(bitangents, 0, sizeof(float) * 4 * count);
    [self _enumerateTriangles:^(uint32_t a, uint32_t b, uint32_t c) {
        if (a >= count || b >= count || c >= count)
            return;
        const float *p0 = positions + 4 * a, *p1 = positions + 4 * b, *p2 = positions + 4 * c;
        const float *t0 = uvs + 4 * a, *t1 = uvs + 4 * b, *t2 = uvs + 4 * c;
        float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
        float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
        float du1 = t1[0] - t0[0], dv1 = t1[1] - t0[1], du2 = t2[0] - t0[0], dv2 = t2[1] - t0[1];
        float det = du1 * dv2 - du2 * dv1;
        if (fabsf(det) < 1e-12f)
            return;
        float r = 1.0f / det;
        float t[3], s[3];
        for (int k = 0; k < 3; k++) {
            t[k] = (e1[k] * dv2 - e2[k] * dv1) * r;
            s[k] = (e2[k] * du1 - e1[k] * du2) * r;
        }
        uint32_t corners[3] = { a, b, c };
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                tangents[4 * corners[j] + k] += t[k];
                bitangents[4 * corners[j] + k] += s[k];
            }
        }
    }];
    free(positions);
    free(uvs);
    return YES;
}

static void normalize3(float *v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (length > 1e-20f) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
}

// tangents from the texture coordinates, made perpendicular to the normals, w the handedness
- (void)addTangentBasisForTextureCoordinateAttributeNamed:(NSString *)textureCoordinateAttributeName
                                     normalAttributeNamed:(NSString *)normalAttributeName
                                    tangentAttributeNamed:(NSString *)tangentAttributeName {
    MDLVertexAttribute *normalAttribute = [_vertexDescriptor attributeNamed:normalAttributeName];
    float *tangents = malloc(sizeof(float) * 4 * (_vertexCount ? _vertexCount : 1));
    float *bitangents = malloc(sizeof(float) * 4 * (_vertexCount ? _vertexCount : 1));

    if (normalAttribute == nil || ![self _accumulateTangents:tangents bitangents:bitangents textureCoordinates:textureCoordinateAttributeName]) {
        free(tangents);
        free(bitangents);
        return;
    }
    float *normals = [self _copyValuesOfAttribute:normalAttribute];
    for (NSUInteger v = 0; v < _vertexCount; v++) {
        float *t = tangents + 4 * v, *b = bitangents + 4 * v, *n = normals + 4 * v;
        float dot = n[0] * t[0] + n[1] * t[1] + n[2] * t[2];
        for (int k = 0; k < 3; k++)
            t[k] -= n[k] * dot;
        normalize3(t);
        float cross[3] = { n[1] * t[2] - n[2] * t[1], n[2] * t[0] - n[0] * t[2], n[0] * t[1] - n[1] * t[0] };
        t[3] = (cross[0] * b[0] + cross[1] * b[1] + cross[2] * b[2]) < 0 ? -1.0f : 1.0f;
    }
    [self _storeValues:tangents forAttribute:tangentAttributeName format:MDLVertexFormatFloat4];
    free(normals);
    free(tangents);
    free(bitangents);
}

// bitangents from the texture coordinates, perpendicular to the tangents the mesh has
- (void)addTangentBasisForTextureCoordinateAttributeNamed:(NSString *)textureCoordinateAttributeName
                                    tangentAttributeNamed:(NSString *)tangentAttributeName
                                  bitangentAttributeNamed:(NSString *)bitangentAttributeName {
    float *tangents = malloc(sizeof(float) * 4 * (_vertexCount ? _vertexCount : 1));
    float *bitangents = malloc(sizeof(float) * 4 * (_vertexCount ? _vertexCount : 1));

    if (![self _accumulateTangents:tangents bitangents:bitangents textureCoordinates:textureCoordinateAttributeName]) {
        free(tangents);
        free(bitangents);
        return;
    }
    MDLVertexAttribute *existing = [_vertexDescriptor attributeNamed:tangentAttributeName];
    if (existing != nil) {
        free(tangents);
        tangents = [self _copyValuesOfAttribute:existing];
    } else {
        for (NSUInteger v = 0; v < _vertexCount; v++) {
            normalize3(tangents + 4 * v);
            tangents[4 * v + 3] = 1.0f;
        }
        [self _storeValues:tangents forAttribute:tangentAttributeName format:MDLVertexFormatFloat3];
    }
    for (NSUInteger v = 0; v < _vertexCount; v++) {
        float *t = tangents + 4 * v, *b = bitangents + 4 * v;
        float dot = t[0] * b[0] + t[1] * b[1] + t[2] * b[2];
        for (int k = 0; k < 3; k++)
            b[k] -= t[k] * dot;
        normalize3(b);
        b[3] = 1.0f;
    }
    [self _storeValues:bitangents forAttribute:bitangentAttributeName format:MDLVertexFormatFloat3];
    free(tangents);
    free(bitangents);
}

// smooth normals from the faces, averaged at each vertex (faces meeting at a sharp angle share none)
- (void)addNormalsWithAttributeNamed:(NSString *)attributeName creaseThreshold:(float)creaseThreshold {
    MDLVertexAttribute *positionAttribute = [_vertexDescriptor attributeNamed:MDLVertexAttributePosition];
    if (positionAttribute == nil)
        return;

    NSString *name = attributeName != nil ? attributeName : MDLVertexAttributeNormal;
    float *positions = [self _copyValuesOfAttribute:positionAttribute];
    float *normals = calloc(4 * (_vertexCount ? _vertexCount : 1), sizeof(float));
    NSUInteger count = _vertexCount;

    [self _enumerateTriangles:^(uint32_t a, uint32_t b, uint32_t c) {
        if (a >= count || b >= count || c >= count)
            return;
        const float *p0 = positions + 4 * a, *p1 = positions + 4 * b, *p2 = positions + 4 * c;
        float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
        float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
        float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
        uint32_t corners[3] = { a, b, c };
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++)
                normals[4 * corners[j] + k] += n[k];
        }
    }];
    for (NSUInteger v = 0; v < _vertexCount; v++) {
        normalize3(normals + 4 * v);
        normals[4 * v + 3] = 0;
    }
    [self _storeValues:normals forAttribute:name format:MDLVertexFormatFloat3];
    free(positions);
    free(normals);
}

- (MDLAxisAlignedBoundingBox)boundingBox {
    MDLAxisAlignedBoundingBox box = { { -INFINITY, -INFINITY, -INFINITY }, { INFINITY, INFINITY, INFINITY } };
    MDLVertexAttribute *attribute = [_vertexDescriptor attributeNamed:MDLVertexAttributePosition];

    if (attribute == nil || _vertexCount == 0)
        return box;
    float *positions = [self _copyValuesOfAttribute:attribute];
    for (NSUInteger v = 0; v < _vertexCount; v++) {
        vector_float3 p = { positions[4 * v], positions[4 * v + 1], positions[4 * v + 2] };
        box.minBounds = (vector_float3){ fminf(box.minBounds.x, p.x), fminf(box.minBounds.y, p.y), fminf(box.minBounds.z, p.z) };
        box.maxBounds = (vector_float3){ fmaxf(box.maxBounds.x, p.x), fmaxf(box.maxBounds.y, p.y), fmaxf(box.maxBounds.z, p.z) };
    }
    free(positions);
    return box;
}

- (MDLAxisAlignedBoundingBox)boundingBoxAtTime:(NSTimeInterval)time {
    return [self boundingBox];
}

@end
