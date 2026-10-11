// MDLAsset.m - objects, assets read from USD crate files, and textures
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import "ModelIO.h"
#import "MDLPrivate.h"
#include "USDCrate.h"
#include <math.h>
#include <string.h>

@implementation MDLObjectContainer {
    NSMutableArray *_objects;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _objects = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc {
    [_objects release];
    [super dealloc];
}

- (void)addObject:(MDLObject *)object { [_objects addObject:object]; }
- (void)removeObject:(MDLObject *)object { [_objects removeObject:object]; }
- (MDLObject *)objectAtIndexedSubscript:(NSUInteger)index { return [_objects objectAtIndex:index]; }
- (NSUInteger)count { return [_objects count]; }
- (NSArray *)objects { return [[_objects copy] autorelease]; }

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id __unsafe_unretained *)buffer count:(NSUInteger)length {
    return [_objects countByEnumeratingWithState:state objects:buffer count:length];
}

@end

@implementation MDLTransform {
    matrix_float4x4 _matrix;
}

- (instancetype)init {
    matrix_float4x4 identity = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
    return [self initWithMatrix:identity];
}

- (instancetype)initWithMatrix:(matrix_float4x4)matrix {
    if ((self = [super init]) != nil)
        _matrix = matrix;
    return self;
}

- (matrix_float4x4)matrix { return _matrix; }
- (void)setMatrix:(matrix_float4x4)matrix { _matrix = matrix; }

@end

@implementation MDLObject {
    NSString *_name, *_path;
    MDLObject *_parent;
    id <MDLObjectContainerComponent> _children;
    id <MDLTransformComponent> _transform;
    BOOL _hidden;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _name = @"";
        _children = [[MDLObjectContainer alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [_path release];
    [_children release];
    [_transform release];
    [super dealloc];
}

- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { NSString *old = _name; _name = [name copy]; [old release]; }
- (void)_setPath:(NSString *)path { NSString *old = _path; _path = [path copy]; [old release]; }
- (MDLObject *)parent { return _parent; }
- (void)setParent:(MDLObject *)parent { _parent = parent; }
- (id <MDLObjectContainerComponent>)children { return _children; }
- (void)setChildren:(id <MDLObjectContainerComponent>)children { [children retain]; [_children release]; _children = children; }
- (id <MDLTransformComponent>)transform { return _transform; }
- (void)setTransform:(id <MDLTransformComponent>)transform { [transform retain]; [_transform release]; _transform = transform; }
- (BOOL)hidden { return _hidden; }
- (void)setHidden:(BOOL)hidden { _hidden = hidden; }

// a path made from the names when nothing set one
- (NSString *)path {
    if (_path != nil)
        return _path;
    return [[_parent path] stringByAppendingFormat:@"/%@", _name] ?: [@"/" stringByAppendingString:_name];
}

- (void)addChild:(MDLObject *)child {
    [child setParent:self];
    [_children addObject:child];
}

- (MDLObject *)objectAtPath:(NSString *)path {
    for (MDLObject *child in _children) {
        if ([[child path] isEqualToString:path])
            return child;
        MDLObject *found = [child objectAtPath:path];
        if (found != nil)
            return found;
    }
    return nil;
}

- (MDLAxisAlignedBoundingBox)boundingBoxAtTime:(NSTimeInterval)time {
    MDLAxisAlignedBoundingBox box = { { -INFINITY, -INFINITY, -INFINITY }, { INFINITY, INFINITY, INFINITY } };

    for (MDLObject *child in _children) {
        MDLAxisAlignedBoundingBox b = [child boundingBoxAtTime:time];
        box.minBounds = (vector_float3){ fminf(box.minBounds.x, b.minBounds.x), fminf(box.minBounds.y, b.minBounds.y), fminf(box.minBounds.z, b.minBounds.z) };
        box.maxBounds = (vector_float3){ fmaxf(box.maxBounds.x, b.maxBounds.x), fmaxf(box.maxBounds.y, b.maxBounds.y), fmaxf(box.maxBounds.z, b.maxBounds.z) };
    }
    return box;
}

@end

// USD crate reading: a prim tree to MDLObjects, Mesh prims to MDLMeshes

@interface MDLUSDReader : NSObject {
@public
    usdc *_file;
    NSMutableDictionary *_specByPath;
    id <MDLMeshBufferAllocator> _allocator;
}
@end

@implementation MDLUSDReader

- (void)dealloc {
    usdc_close(_file);
    [_specByPath release];
    [super dealloc];
}

- (BOOL)field:(NSString *)field ofPath:(NSString *)path value:(usdc_value *)value {
    NSNumber *spec = [_specByPath objectForKey:path];

    return spec != nil && usdc_spec_field(_file, [spec unsignedIntegerValue], [field UTF8String], value);
}

- (NSString *)tokenField:(NSString *)field ofPath:(NSString *)path {
    usdc_value value;

    if (![self field:field ofPath:path value:&value])
        return nil;
    NSString *result = value.string != NULL ? [NSString stringWithUTF8String:value.string] : nil;
    usdc_value_free(&value);
    return result;
}

- (NSArray *)childNamesOfPath:(NSString *)path {
    usdc_value value;
    NSMutableArray *names = [NSMutableArray array];

    if ([self field:@"primChildren" ofPath:path value:&value]) {
        for (size_t i = 0; i < value.count && value.strings != NULL; i++)
            [names addObject:[NSString stringWithUTF8String:value.strings[i]]];
        usdc_value_free(&value);
    }
    return names;
}

// an Xform's local matrix, USD's rows becoming ModelIO's columns
- (MDLTransform *)transformOfPath:(NSString *)path {
    usdc_value value;

    if (![self field:@"default" ofPath:[path stringByAppendingString:@".xformOp:transform"] value:&value])
        return nil;
    if (value.scalarCount != 16) {
        usdc_value_free(&value);
        return nil;
    }
    matrix_float4x4 m;
    for (int c = 0; c < 4; c++)
        m.columns[c] = (vector_float4){ (float)value.scalars[c * 4], (float)value.scalars[c * 4 + 1], (float)value.scalars[c * 4 + 2], (float)value.scalars[c * 4 + 3] };
    usdc_value_free(&value);
    return [[[MDLTransform alloc] initWithMatrix:m] autorelease];
}

- (MDLObject *)objectForPath:(NSString *)path name:(NSString *)name {
    NSString *type = [self tokenField:@"typeName" ofPath:path];
    MDLObject *object = [type isEqualToString:@"Mesh"] ? [self meshForPath:path] : nil;

    if (object == nil)
        object = [[[MDLObject alloc] init] autorelease];
    [object setName:name];
    [object _setPath:path];
    [object setTransform:[self transformOfPath:path]];
    for (NSString *childName in [self childNamesOfPath:path]) {
        MDLObject *child = [self objectForPath:[path stringByAppendingFormat:@"/%@", childName] name:childName];
        [object addChild:child];
    }
    return object;
}

// the attribute's default value as an array of the given type, or NO
- (BOOL)array:(NSString *)attribute ofMesh:(NSString *)path type:(int)type value:(usdc_value *)value {
    if (![self field:@"default" ofPath:[NSString stringWithFormat:@"%@.%@", path, attribute] value:value])
        return NO;
    if (!value->isArray || value->type != type || value->data == NULL) {
        usdc_value_free(value);
        return NO;
    }
    return YES;
}

// the first texCoord2f-style primvar, as ModelIO's textureCoordinate
- (NSString *)textureCoordinatePrimvarOfMesh:(NSString *)path {
    usdc_value properties;
    NSString *found = nil;

    if (![self field:@"properties" ofPath:path value:&properties])
        return nil;
    for (size_t i = 0; i < properties.count && properties.strings != NULL && found == nil; i++) {
        NSString *name = [NSString stringWithUTF8String:properties.strings[i]];
        if (![name hasPrefix:@"primvars:"] || [name hasSuffix:@":indices"])
            continue;
        NSString *typeName = [self tokenField:@"typeName" ofPath:[NSString stringWithFormat:@"%@.%@", path, name]];
        if ([typeName isEqualToString:@"texCoord2f[]"] || [typeName isEqualToString:@"float2[]"])
            found = name;
    }
    usdc_value_free(&properties);
    return found;
}

// a vertex per point, then one more for each later corner of a claimed point when an attribute
// varies per corner, which is how ModelIO numbers them
- (MDLMesh *)meshForPath:(NSString *)path {
    usdc_value points, counts, indices, normals, uvs, uvIndices;
    BOOL haveNormals, haveUVs, haveUVIndices = NO;

    if (![self array:@"points" ofMesh:path type:USDC_TYPE_VEC3F value:&points])
        return nil;
    if (![self array:@"faceVertexCounts" ofMesh:path type:USDC_TYPE_INT value:&counts]) {
        usdc_value_free(&points);
        return nil;
    }
    if (![self array:@"faceVertexIndices" ofMesh:path type:USDC_TYPE_INT value:&indices]) {
        usdc_value_free(&points);
        usdc_value_free(&counts);
        return nil;
    }
    haveNormals = [self array:@"normals" ofMesh:path type:USDC_TYPE_VEC3F value:&normals];
    NSString *uvName = [self textureCoordinatePrimvarOfMesh:path];
    haveUVs = uvName != nil && [self array:uvName ofMesh:path type:USDC_TYPE_VEC2F value:&uvs];
    if (haveUVs)
        haveUVIndices = [self array:[uvName stringByAppendingString:@":indices"] ofMesh:path type:USDC_TYPE_INT value:&uvIndices];

    NSString *normalInterpolation = haveNormals ? [self tokenField:@"interpolation" ofPath:[path stringByAppendingString:@".normals"]] : nil;
    NSString *uvInterpolation = haveUVs ? [self tokenField:@"interpolation" ofPath:[NSString stringWithFormat:@"%@.%@", path, uvName]] : nil;
    BOOL normalsPerCorner = haveNormals && normals.count == indices.count && ![normalInterpolation isEqualToString:@"vertex"];
    BOOL uvsPerCorner = haveUVs && ([uvInterpolation isEqualToString:@"faceVarying"] || (haveUVIndices ? uvIndices.count : uvs.count) == indices.count);
    BOOL split = normalsPerCorner || uvsPerCorner;
    BOOL leftHanded = [[self tokenField:@"orientation" ofPath:path] isEqualToString:@"leftHanded"];

    const float *p = points.data;
    const int32_t *faceCounts = counts.data, *faceIndices = indices.data;
    size_t pointCount = points.count, cornerCount = indices.count;
    size_t maxVertices = pointCount + cornerCount;
    uint32_t *cornerVertex = malloc(sizeof(uint32_t) * (cornerCount ? cornerCount : 1));
    uint8_t *claimed = calloc(pointCount ? pointCount : 1, 1);
    float *outPositions = calloc(maxVertices * 3 + 1, sizeof(float));
    float *outNormals = calloc(maxVertices * 3 + 1, sizeof(float));
    float *outUVs = calloc(maxVertices * 2 + 1, sizeof(float));
    size_t vertexCount = pointCount;
    BOOL ok = YES;

    for (size_t v = 0; v < pointCount; v++)
        memcpy(outPositions + 3 * v, p + 3 * v, 3 * sizeof(float));

    for (size_t c = 0; c < cornerCount && ok; c++) {
        int32_t point = faceIndices[c];
        if (point < 0 || (size_t)point >= pointCount) {
            ok = NO;
            break;
        }
        uint32_t vertex = (uint32_t)point;
        if (split && claimed[point])
            vertex = (uint32_t)vertexCount++;
        claimed[point] = 1;
        cornerVertex[c] = vertex;
        memcpy(outPositions + 3 * vertex, p + 3 * point, 3 * sizeof(float));

        if (haveNormals) {
            size_t n = normalsPerCorner ? c : (size_t)point;
            if (n < normals.count)
                memcpy(outNormals + 3 * vertex, (const float *)normals.data + 3 * n, 3 * sizeof(float));
        }
        if (haveUVs) {
            size_t u = uvsPerCorner ? c : (size_t)point;
            if (haveUVIndices)
                u = u < uvIndices.count ? (size_t)((const int32_t *)uvIndices.data)[u] : uvs.count;
            if (u < uvs.count)
                memcpy(outUVs + 2 * vertex, (const float *)uvs.data + 2 * u, 2 * sizeof(float));
        }
    }

    // quads split along the shorter diagonal, other polygons as fans from their first corner
    NSMutableData *triangles = [NSMutableData data];
    size_t corner = 0;
    for (size_t f = 0; f < counts.count && ok; f++) {
        int32_t n = faceCounts[f];
        if (n < 3 || corner + (size_t)n > cornerCount) {
            corner += n > 0 ? (size_t)n : 0;
            continue;
        }
        const uint32_t *fv = cornerVertex + corner;
        uint32_t tri[6];
        int triCount = 0;
        if (n == 4) {
            const float *a = outPositions + 3 * fv[0], *b = outPositions + 3 * fv[1];
            const float *c2 = outPositions + 3 * fv[2], *d = outPositions + 3 * fv[3];
            float d02 = (a[0] - c2[0]) * (a[0] - c2[0]) + (a[1] - c2[1]) * (a[1] - c2[1]) + (a[2] - c2[2]) * (a[2] - c2[2]);
            float d13 = (b[0] - d[0]) * (b[0] - d[0]) + (b[1] - d[1]) * (b[1] - d[1]) + (b[2] - d[2]) * (b[2] - d[2]);
            if (d02 < d13) {
                uint32_t t[6] = { fv[0], fv[1], fv[2], fv[0], fv[2], fv[3] };
                memcpy(tri, t, sizeof(t));
            } else {
                uint32_t t[6] = { fv[0], fv[1], fv[3], fv[1], fv[2], fv[3] };
                memcpy(tri, t, sizeof(t));
            }
            triCount = 2;
            for (int t = 0; t < triCount; t++) {
                if (leftHanded) {
                    uint32_t s = tri[3 * t + 1];
                    tri[3 * t + 1] = tri[3 * t + 2];
                    tri[3 * t + 2] = s;
                }
                [triangles appendBytes:tri + 3 * t length:3 * sizeof(uint32_t)];
            }
        } else {
            for (int32_t k = 1; k + 1 < n; k++) {
                uint32_t t[3] = { fv[0], fv[k], fv[k + 1] };
                if (leftHanded) {
                    t[1] = fv[k + 1];
                    t[2] = fv[k];
                }
                [triangles appendBytes:t length:sizeof(t)];
            }
        }
        corner += (size_t)n;
    }

    MDLMesh *mesh = nil;
    if (ok) {
        MDLVertexDescriptor *descriptor = [[[MDLVertexDescriptor alloc] init] autorelease];
        NSMutableArray *buffers = [NSMutableArray array];
        NSUInteger buffer = 0;

        [descriptor.attributes replaceObjectAtIndex:0 withObject:[[[MDLVertexAttribute alloc] initWithName:MDLVertexAttributePosition format:MDLVertexFormatFloat3 offset:0 bufferIndex:buffer] autorelease]];
        [descriptor.layouts[buffer] setStride:12];
        [buffers addObject:[[_allocator newBufferWithData:[NSData dataWithBytes:outPositions length:vertexCount * 12] type:MDLMeshBufferTypeVertex] autorelease]];
        buffer++;
        if (haveNormals) {
            [descriptor.attributes replaceObjectAtIndex:1 withObject:[[[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeNormal format:MDLVertexFormatFloat3 offset:0 bufferIndex:buffer] autorelease]];
            [descriptor.layouts[buffer] setStride:12];
            [buffers addObject:[[_allocator newBufferWithData:[NSData dataWithBytes:outNormals length:vertexCount * 12] type:MDLMeshBufferTypeVertex] autorelease]];
            buffer++;
        }
        if (haveUVs) {
            [descriptor.attributes replaceObjectAtIndex:2 withObject:[[[MDLVertexAttribute alloc] initWithName:MDLVertexAttributeTextureCoordinate format:MDLVertexFormatFloat2 offset:0 bufferIndex:buffer] autorelease]];
            [descriptor.layouts[buffer] setStride:8];
            [buffers addObject:[[_allocator newBufferWithData:[NSData dataWithBytes:outUVs length:vertexCount * 8] type:MDLMeshBufferTypeVertex] autorelease]];
        }

        id <MDLMeshBuffer> indexBuffer = [[_allocator newBufferWithData:triangles type:MDLMeshBufferTypeIndex] autorelease];
        MDLMaterial *material = [[[MDLMaterial alloc] initWithName:@"material" scatteringFunction:nil] autorelease];
        MDLSubmesh *submesh = [[[MDLSubmesh alloc] initWithName:@"submesh" indexBuffer:indexBuffer indexCount:[triangles length] / 4
                                                      indexType:MDLIndexBitDepthUInt32 geometryType:MDLGeometryTypeTriangles material:material] autorelease];
        mesh = [[[MDLMesh alloc] initWithVertexBuffers:buffers vertexCount:vertexCount descriptor:descriptor submeshes:@[ submesh ]] autorelease];
    }

    free(cornerVertex);
    free(claimed);
    free(outPositions);
    free(outNormals);
    free(outUVs);
    usdc_value_free(&points);
    usdc_value_free(&counts);
    usdc_value_free(&indices);
    if (haveNormals)
        usdc_value_free(&normals);
    if (haveUVs)
        usdc_value_free(&uvs);
    if (haveUVIndices)
        usdc_value_free(&uvIndices);
    return mesh;
}

@end

@implementation MDLAsset {
    NSMutableArray *_objects;
    NSURL *_URL;
    id <MDLMeshBufferAllocator> _bufferAllocator;
    MDLVertexDescriptor *_vertexDescriptor;
    vector_float3 _upAxis;
    NSTimeInterval _startTime, _endTime, _frameInterval;
}

+ (BOOL)canImportFileExtension:(NSString *)extension {
    return [[extension lowercaseString] isEqualToString:@"usdc"] || [[extension lowercaseString] isEqualToString:@"usd"];
}

+ (BOOL)canExportFileExtension:(NSString *)extension {
    return NO;
}

- (instancetype)init {
    return [self initWithBufferAllocator:nil];
}

- (instancetype)initWithBufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator {
    if ((self = [super init]) != nil) {
        _objects = [[NSMutableArray alloc] init];
        _bufferAllocator = bufferAllocator != nil ? [bufferAllocator retain] : [[MDLMeshBufferDataAllocator alloc] init];
        _upAxis = (vector_float3){ 0, 1, 0 };
    }
    return self;
}

- (instancetype)initWithURL:(NSURL *)URL {
    return [self initWithURL:URL vertexDescriptor:nil bufferAllocator:nil preserveTopology:NO error:NULL];
}

- (instancetype)initWithURL:(NSURL *)URL vertexDescriptor:(MDLVertexDescriptor *)vertexDescriptor
            bufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator {
    return [self initWithURL:URL vertexDescriptor:vertexDescriptor bufferAllocator:bufferAllocator preserveTopology:NO error:NULL];
}

// a file that cannot be read gives nil and, when asked, the reason
- (instancetype)initWithURL:(NSURL *)URL vertexDescriptor:(MDLVertexDescriptor *)vertexDescriptor
            bufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator preserveTopology:(BOOL)preserveTopology
                      error:(NSError **)error {
    if ((self = [self initWithBufferAllocator:bufferAllocator]) == nil)
        return nil;
    _URL = [URL copy];
    _vertexDescriptor = [vertexDescriptor copy];

    NSData *data = [NSData dataWithContentsOfURL:URL options:NSDataReadingMappedIfSafe error:error];
    const char *reason = "file could not be read";
    usdc *file = data != nil ? usdc_open([data bytes], [data length], &reason) : NULL;
    if (file == NULL) {
        NSLog(@"MDLAsset: %@: %s", [URL path], reason);
        if (error != NULL && *error == nil)
            *error = [NSError errorWithDomain:NSCocoaErrorDomain code:259
                                     userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithUTF8String:reason] }];
        [self release];
        return nil;
    }

    MDLUSDReader *reader = [[[MDLUSDReader alloc] init] autorelease];
    reader->_file = file;
    reader->_allocator = _bufferAllocator;
    reader->_specByPath = [[NSMutableDictionary alloc] init];
    for (size_t i = 0; i < usdc_spec_count(file); i++) {
        const char *path = usdc_spec_path(file, i);
        if (path != NULL)
            [reader->_specByPath setObject:[NSNumber numberWithUnsignedLong:i] forKey:[NSString stringWithUTF8String:path]];
    }

    NSString *up = [reader tokenField:@"upAxis" ofPath:@"/"];
    if ([up isEqualToString:@"Z"])
        _upAxis = (vector_float3){ 0, 0, 1 };

    for (NSString *name in [reader childNamesOfPath:@"/"])
        [_objects addObject:[reader objectForPath:[@"/" stringByAppendingString:name] name:name]];

    // a descriptor given here is applied to every mesh, as ModelIO does on import
    if (_vertexDescriptor != nil) {
        for (MDLObject *object in [self childObjectsOfClass:[MDLMesh class]])
            [(MDLMesh *)object setVertexDescriptor:_vertexDescriptor];
    }
    // the data is mapped, so the reader goes before it does
    reader->_file = NULL;
    usdc_close(file);
    return self;
}

- (void)dealloc {
    [_objects release];
    [_URL release];
    [_bufferAllocator release];
    [_vertexDescriptor release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    MDLAsset *copy = [[MDLAsset alloc] initWithBufferAllocator:_bufferAllocator];
    [copy->_objects addObjectsFromArray:_objects];
    copy->_URL = [_URL copy];
    copy->_upAxis = _upAxis;
    return copy;
}

- (void)addObject:(MDLObject *)object { [_objects addObject:object]; }
- (void)removeObject:(MDLObject *)object { [_objects removeObject:object]; }
- (MDLObject *)objectAtIndex:(NSUInteger)index { return [_objects objectAtIndex:index]; }
- (MDLObject *)objectAtIndexedSubscript:(NSUInteger)index { return [_objects objectAtIndex:index]; }
- (NSUInteger)count { return [_objects count]; }
- (NSURL *)URL { return _URL; }
- (id <MDLMeshBufferAllocator>)bufferAllocator { return _bufferAllocator; }
- (MDLVertexDescriptor *)vertexDescriptor { return _vertexDescriptor; }
- (vector_float3)upAxis { return _upAxis; }
- (void)setUpAxis:(vector_float3)axis { _upAxis = axis; }
- (NSTimeInterval)startTime { return _startTime; }
- (void)setStartTime:(NSTimeInterval)time { _startTime = time; }
- (NSTimeInterval)endTime { return _endTime; }
- (void)setEndTime:(NSTimeInterval)time { _endTime = time; }
- (NSTimeInterval)frameInterval { return _frameInterval; }
- (void)setFrameInterval:(NSTimeInterval)interval { _frameInterval = interval; }

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id __unsafe_unretained *)buffer count:(NSUInteger)length {
    return [_objects countByEnumeratingWithState:state objects:buffer count:length];
}

- (MDLObject *)objectAtPath:(NSString *)path {
    for (MDLObject *object in _objects) {
        if ([[object path] isEqualToString:path])
            return object;
        MDLObject *found = [object objectAtPath:path];
        if (found != nil)
            return found;
    }
    return nil;
}

static void collect(MDLObject *object, Class objectClass, NSMutableArray *into) {
    if ([object isKindOfClass:objectClass])
        [into addObject:object];
    for (MDLObject *child in object.children)
        collect(child, objectClass, into);
}

- (NSArray<MDLObject *> *)childObjectsOfClass:(Class)objectClass {
    NSMutableArray *result = [NSMutableArray array];

    for (MDLObject *object in _objects)
        collect(object, objectClass, result);
    return result;
}

- (MDLAxisAlignedBoundingBox)boundingBox {
    MDLAxisAlignedBoundingBox box = { { -INFINITY, -INFINITY, -INFINITY }, { INFINITY, INFINITY, INFINITY } };

    for (MDLObject *object in [self childObjectsOfClass:[MDLMesh class]]) {
        MDLAxisAlignedBoundingBox b = [(MDLMesh *)object boundingBox];
        box.minBounds = (vector_float3){ fminf(box.minBounds.x, b.minBounds.x), fminf(box.minBounds.y, b.minBounds.y), fminf(box.minBounds.z, b.minBounds.z) };
        box.maxBounds = (vector_float3){ fmaxf(box.maxBounds.x, b.maxBounds.x), fmaxf(box.maxBounds.y, b.maxBounds.y), fmaxf(box.maxBounds.z, b.maxBounds.z) };
    }
    return box;
}

@end

// textures: images decoded to 8-bit RGBA, a cube's six faces stacked top to bottom

@implementation MDLTexture {
    NSData *_texels;
    vector_int2 _dimensions;
    NSInteger _rowStride;
    NSUInteger _channelCount;
    MDLTextureChannelEncoding _encoding;
    BOOL _isCube, _hasAlpha;
    NSString *_name;
}

- (instancetype)initWithData:(NSData *)pixelData topLeftOrigin:(BOOL)topLeftOrigin name:(NSString *)name
                  dimensions:(vector_int2)dimensions rowStride:(NSInteger)rowStride channelCount:(NSUInteger)channelCount
             channelEncoding:(MDLTextureChannelEncoding)channelEncoding isCube:(BOOL)isCube {
    if ((self = [super init]) == nil)
        return nil;

    NSInteger rows = dimensions.y * (isCube ? 6 : 1);
    if (topLeftOrigin || pixelData == nil) {
        _texels = [pixelData copy];
    } else {
        NSMutableData *flipped = [NSMutableData dataWithLength:[pixelData length]];
        for (NSInteger r = 0; r < rows; r++)
            memcpy((uint8_t *)[flipped mutableBytes] + r * rowStride, (const uint8_t *)[pixelData bytes] + (rows - 1 - r) * rowStride, rowStride);
        _texels = [flipped copy];
    }
    _name = [name copy];
    _dimensions = dimensions;
    _rowStride = rowStride;
    _channelCount = channelCount;
    _encoding = channelEncoding;
    _isCube = isCube;
    return self;
}

- (void)dealloc {
    [_texels release];
    [_name release];
    [super dealloc];
}

static NSString *imagePath(NSString *name, NSBundle *bundle) {
    NSBundle *where = bundle != nil ? bundle : [NSBundle mainBundle];
    NSString *path = [where pathForResource:[name stringByDeletingPathExtension] ofType:[name pathExtension]];

    if (path == nil && [[name pathExtension] length] == 0)
        path = [where pathForResource:name ofType:@"png"];
    if (path == nil && [name isAbsolutePath])
        path = name;
    return path;
}

// 8-bit RGBA, top row first, through CoreGraphics' image sources
static NSData *decodeImage(NSString *path, int *width, int *height) {
    NSURL *url = [NSURL fileURLWithPath:path];
    CGImageSourceRef source = CGImageSourceCreateWithURL((CFURLRef)url, NULL);
    if (source == NULL)
        return nil;
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
    CFRelease(source);
    if (image == NULL)
        return nil;

    size_t w = CGImageGetWidth(image), h = CGImageGetHeight(image);
    NSMutableData *pixels = [NSMutableData dataWithLength:w * h * 4];
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate([pixels mutableBytes], w, h, 8, w * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if (context == NULL) {
        CGImageRelease(image);
        return nil;
    }
    CGContextDrawImage(context, CGRectMake(0, 0, w, h), image);
    CGContextRelease(context);
    CGImageRelease(image);
    *width = (int)w;
    *height = (int)h;
    return pixels;
}

+ (instancetype)textureNamed:(NSString *)name {
    return [self textureNamed:name bundle:nil];
}

+ (instancetype)textureNamed:(NSString *)name bundle:(NSBundle *)bundleOrNil {
    NSString *path = imagePath(name, bundleOrNil);
    int w = 0, h = 0;
    NSData *pixels = path != nil ? decodeImage(path, &w, &h) : nil;

    if (pixels == nil)
        return nil;
    return [[[self alloc] initWithData:pixels topLeftOrigin:YES name:name dimensions:(vector_int2){ w, h } rowStride:w * 4
                          channelCount:4 channelEncoding:MDLTextureChannelEncodingUInt8 isCube:NO] autorelease];
}

+ (instancetype)textureCubeWithImagesNamed:(NSArray<NSString *> *)names {
    return [self textureCubeWithImagesNamed:names bundle:nil];
}

// one image is a vertical strip of the six faces, six images are the faces in +X -X +Y -Y +Z -Z order
+ (instancetype)textureCubeWithImagesNamed:(NSArray<NSString *> *)names bundle:(NSBundle *)bundleOrNil {
    NSMutableData *faces = [NSMutableData data];
    int size = 0;

    for (NSString *name in names) {
        NSString *path = imagePath(name, bundleOrNil);
        int w = 0, h = 0;
        NSData *pixels = path != nil ? decodeImage(path, &w, &h) : nil;
        if (pixels == nil)
            return nil;
        if ([names count] == 1 && h != 6 * w)
            return nil;
        if ([names count] == 6 && (w != h || (size != 0 && w != size)))
            return nil;
        size = w;
        [faces appendData:pixels];
    }
    if (([names count] != 1 && [names count] != 6) || size == 0)
        return nil;
    return [[[self alloc] initWithData:faces topLeftOrigin:YES name:[names firstObject] dimensions:(vector_int2){ size, size }
                             rowStride:size * 4 channelCount:4 channelEncoding:MDLTextureChannelEncodingUInt8 isCube:YES] autorelease];
}

- (NSData *)texelDataWithTopLeftOrigin {
    return _texels;
}

- (NSData *)texelDataWithBottomLeftOrigin {
    NSInteger rows = _dimensions.y * (_isCube ? 6 : 1);
    NSMutableData *flipped = [NSMutableData dataWithLength:[_texels length]];

    for (NSInteger r = 0; r < rows; r++)
        memcpy((uint8_t *)[flipped mutableBytes] + r * _rowStride, (const uint8_t *)[_texels bytes] + (rows - 1 - r) * _rowStride, _rowStride);
    return flipped;
}

- (NSData *)texelDataWithTopLeftOriginAtMipLevel:(NSInteger)level create:(BOOL)create {
    return level == 0 ? [self texelDataWithTopLeftOrigin] : nil;
}

- (NSData *)texelDataWithBottomLeftOriginAtMipLevel:(NSInteger)level create:(BOOL)create {
    return level == 0 ? [self texelDataWithBottomLeftOrigin] : nil;
}

- (vector_int2)dimensions { return _dimensions; }
- (NSInteger)rowStride { return _rowStride; }
- (NSUInteger)channelCount { return _channelCount; }
- (NSUInteger)mipLevelCount { return 1; }
- (MDLTextureChannelEncoding)channelEncoding { return _encoding; }
- (BOOL)isCube { return _isCube; }
- (void)setIsCube:(BOOL)flag { _isCube = flag; }
- (BOOL)hasAlphaValues { return _hasAlpha; }
- (void)setHasAlphaValues:(BOOL)flag { _hasAlpha = flag; }
- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { NSString *old = _name; _name = [name copy]; [old release]; }

@end
