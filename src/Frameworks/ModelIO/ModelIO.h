// ModelIO.h - assets, meshes, vertex descriptors and textures
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef MODELIO_MODELIO_H
#define MODELIO_MODELIO_H

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

#if __has_include(<simd/simd.h>)
#include <simd/simd.h>
#else
typedef float vector_float2 __attribute__((ext_vector_type(2)));
typedef float vector_float3 __attribute__((ext_vector_type(3)));
typedef float vector_float4 __attribute__((ext_vector_type(4)));
typedef int32_t vector_int2 __attribute__((ext_vector_type(2)));
typedef struct { vector_float4 columns[4]; } matrix_float4x4;
#endif

typedef NS_ENUM(NSUInteger, MDLVertexFormat) {
    MDLVertexFormatInvalid = 0,
    MDLVertexFormatPackedBit = 0x1000,
    MDLVertexFormatUCharBits = 0x10000,
    MDLVertexFormatCharBits = 0x20000,
    MDLVertexFormatUCharNormalizedBits = 0x30000,
    MDLVertexFormatCharNormalizedBits = 0x40000,
    MDLVertexFormatUShortBits = 0x50000,
    MDLVertexFormatShortBits = 0x60000,
    MDLVertexFormatUShortNormalizedBits = 0x70000,
    MDLVertexFormatShortNormalizedBits = 0x80000,
    MDLVertexFormatUIntBits = 0x90000,
    MDLVertexFormatIntBits = 0xA0000,
    MDLVertexFormatHalfBits = 0xB0000,
    MDLVertexFormatFloatBits = 0xC0000,

    MDLVertexFormatUChar = MDLVertexFormatUCharBits | 1,
    MDLVertexFormatUChar2 = MDLVertexFormatUCharBits | 2,
    MDLVertexFormatUChar3 = MDLVertexFormatUCharBits | 3,
    MDLVertexFormatUChar4 = MDLVertexFormatUCharBits | 4,
    MDLVertexFormatChar = MDLVertexFormatCharBits | 1,
    MDLVertexFormatChar2 = MDLVertexFormatCharBits | 2,
    MDLVertexFormatChar3 = MDLVertexFormatCharBits | 3,
    MDLVertexFormatChar4 = MDLVertexFormatCharBits | 4,
    MDLVertexFormatUCharNormalized = MDLVertexFormatUCharNormalizedBits | 1,
    MDLVertexFormatUChar2Normalized = MDLVertexFormatUCharNormalizedBits | 2,
    MDLVertexFormatUChar3Normalized = MDLVertexFormatUCharNormalizedBits | 3,
    MDLVertexFormatUChar4Normalized = MDLVertexFormatUCharNormalizedBits | 4,
    MDLVertexFormatCharNormalized = MDLVertexFormatCharNormalizedBits | 1,
    MDLVertexFormatChar2Normalized = MDLVertexFormatCharNormalizedBits | 2,
    MDLVertexFormatChar3Normalized = MDLVertexFormatCharNormalizedBits | 3,
    MDLVertexFormatChar4Normalized = MDLVertexFormatCharNormalizedBits | 4,
    MDLVertexFormatUShort = MDLVertexFormatUShortBits | 1,
    MDLVertexFormatUShort2 = MDLVertexFormatUShortBits | 2,
    MDLVertexFormatUShort3 = MDLVertexFormatUShortBits | 3,
    MDLVertexFormatUShort4 = MDLVertexFormatUShortBits | 4,
    MDLVertexFormatShort = MDLVertexFormatShortBits | 1,
    MDLVertexFormatShort2 = MDLVertexFormatShortBits | 2,
    MDLVertexFormatShort3 = MDLVertexFormatShortBits | 3,
    MDLVertexFormatShort4 = MDLVertexFormatShortBits | 4,
    MDLVertexFormatUShortNormalized = MDLVertexFormatUShortNormalizedBits | 1,
    MDLVertexFormatUShort2Normalized = MDLVertexFormatUShortNormalizedBits | 2,
    MDLVertexFormatUShort3Normalized = MDLVertexFormatUShortNormalizedBits | 3,
    MDLVertexFormatUShort4Normalized = MDLVertexFormatUShortNormalizedBits | 4,
    MDLVertexFormatShortNormalized = MDLVertexFormatShortNormalizedBits | 1,
    MDLVertexFormatShort2Normalized = MDLVertexFormatShortNormalizedBits | 2,
    MDLVertexFormatShort3Normalized = MDLVertexFormatShortNormalizedBits | 3,
    MDLVertexFormatShort4Normalized = MDLVertexFormatShortNormalizedBits | 4,
    MDLVertexFormatUInt = MDLVertexFormatUIntBits | 1,
    MDLVertexFormatUInt2 = MDLVertexFormatUIntBits | 2,
    MDLVertexFormatUInt3 = MDLVertexFormatUIntBits | 3,
    MDLVertexFormatUInt4 = MDLVertexFormatUIntBits | 4,
    MDLVertexFormatInt = MDLVertexFormatIntBits | 1,
    MDLVertexFormatInt2 = MDLVertexFormatIntBits | 2,
    MDLVertexFormatInt3 = MDLVertexFormatIntBits | 3,
    MDLVertexFormatInt4 = MDLVertexFormatIntBits | 4,
    MDLVertexFormatHalf = MDLVertexFormatHalfBits | 1,
    MDLVertexFormatHalf2 = MDLVertexFormatHalfBits | 2,
    MDLVertexFormatHalf3 = MDLVertexFormatHalfBits | 3,
    MDLVertexFormatHalf4 = MDLVertexFormatHalfBits | 4,
    MDLVertexFormatFloat = MDLVertexFormatFloatBits | 1,
    MDLVertexFormatFloat2 = MDLVertexFormatFloatBits | 2,
    MDLVertexFormatFloat3 = MDLVertexFormatFloatBits | 3,
    MDLVertexFormatFloat4 = MDLVertexFormatFloatBits | 4,
    MDLVertexFormatInt1010102Normalized = MDLVertexFormatIntBits | MDLVertexFormatPackedBit | 4,
    MDLVertexFormatUInt1010102Normalized = MDLVertexFormatUIntBits | MDLVertexFormatPackedBit | 4,
};

typedef NS_ENUM(NSUInteger, MDLMeshBufferType) {
    MDLMeshBufferTypeVertex = 1,
    MDLMeshBufferTypeIndex = 2,
    MDLMeshBufferTypeCustom = 3,
};

typedef NS_ENUM(NSUInteger, MDLIndexBitDepth) {
    MDLIndexBitDepthInvalid = 0,
    MDLIndexBitDepthUInt8 = 8,
    MDLIndexBitDepthUint8 = 8,
    MDLIndexBitDepthUInt16 = 16,
    MDLIndexBitDepthUint16 = 16,
    MDLIndexBitDepthUInt32 = 32,
    MDLIndexBitDepthUint32 = 32,
};

typedef NS_ENUM(NSInteger, MDLGeometryType) {
    MDLGeometryTypePoints = 0,
    MDLGeometryTypeLines,
    MDLGeometryTypeTriangles,
    MDLGeometryTypeTriangleStrips,
    MDLGeometryTypeQuads,
    MDLGeometryTypeVariableTopology,
};

typedef NS_ENUM(NSInteger, MDLTextureChannelEncoding) {
    MDLTextureChannelEncodingUInt8 = 1,
    MDLTextureChannelEncodingUint8 = 1,
    MDLTextureChannelEncodingUInt16 = 2,
    MDLTextureChannelEncodingUint16 = 2,
    MDLTextureChannelEncodingUInt24 = 3,
    MDLTextureChannelEncodingUint24 = 3,
    MDLTextureChannelEncodingUInt32 = 4,
    MDLTextureChannelEncodingUint32 = 4,
    MDLTextureChannelEncodingFloat16 = 0x102,
    MDLTextureChannelEncodingFloat16SR = 0x302,
    MDLTextureChannelEncodingFloat32 = 0x104,
};

typedef struct {
    vector_float3 maxBounds;
    vector_float3 minBounds;
} MDLAxisAlignedBoundingBox;

FOUNDATION_EXPORT NSString * const MDLVertexAttributeAnisotropy;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeBinormal;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeBitangent;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeColor;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeEdgeCrease;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeJointIndices;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeJointWeights;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeNormal;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeOcclusionValue;
FOUNDATION_EXPORT NSString * const MDLVertexAttributePosition;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeShadingBasisU;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeShadingBasisV;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeSubdivisionStencil;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeTangent;
FOUNDATION_EXPORT NSString * const MDLVertexAttributeTextureCoordinate;

@class MDLMeshBufferZoneDefault, MDLObject, MDLMaterial, MDLMesh;

@interface MDLMeshBufferMap : NSObject
- (instancetype)initWithBytes:(void *)bytes deallocator:(void (^)(void))deallocator;
@property (readonly) void *bytes;
@end

@protocol MDLMeshBufferAllocator;

@protocol MDLMeshBufferZone <NSObject>
@property (readonly) NSUInteger capacity;
@property (readonly, retain) id <MDLMeshBufferAllocator> allocator;
@end

@protocol MDLMeshBuffer <NSObject, NSCopying>
- (void)fillData:(NSData *)data offset:(NSUInteger)offset;
- (MDLMeshBufferMap *)map;
@property (readonly) NSUInteger length;
@property (readonly, retain) id <MDLMeshBufferAllocator> allocator;
@property (readonly, retain) id <MDLMeshBufferZone> zone;
@property (readonly) MDLMeshBufferType type;
@end

@protocol MDLMeshBufferAllocator <NSObject>
- (id <MDLMeshBufferZone>)newZone:(NSUInteger)capacity;
- (id <MDLMeshBufferZone>)newZoneForBuffersWithSize:(NSArray<NSNumber *> *)sizes andType:(NSArray<NSNumber *> *)types;
- (id <MDLMeshBuffer>)newBuffer:(NSUInteger)length type:(MDLMeshBufferType)type;
- (id <MDLMeshBuffer>)newBufferWithData:(NSData *)data type:(MDLMeshBufferType)type;
- (id <MDLMeshBuffer>)newBufferFromZone:(id <MDLMeshBufferZone>)zone length:(NSUInteger)length type:(MDLMeshBufferType)type;
- (id <MDLMeshBuffer>)newBufferFromZone:(id <MDLMeshBufferZone>)zone data:(NSData *)data type:(MDLMeshBufferType)type;
@end

@interface MDLMeshBufferData : NSObject <MDLMeshBuffer>
- (instancetype)initWithType:(MDLMeshBufferType)type length:(NSUInteger)length;
- (instancetype)initWithType:(MDLMeshBufferType)type data:(NSData *)data;
@property (readonly, retain) NSData *data;
@end

@interface MDLMeshBufferDataAllocator : NSObject <MDLMeshBufferAllocator>
@end

@interface MDLMeshBufferZoneDefault : NSObject <MDLMeshBufferZone>
@end

@interface MDLVertexAttribute : NSObject <NSCopying>
- (instancetype)initWithName:(NSString *)name format:(MDLVertexFormat)format offset:(NSUInteger)offset bufferIndex:(NSUInteger)bufferIndex;
@property (copy) NSString *name;
@property MDLVertexFormat format;
@property NSUInteger offset;
@property NSUInteger bufferIndex;
@property NSTimeInterval time;
@property vector_float4 initializationValue;
@end

@interface MDLVertexBufferLayout : NSObject <NSCopying>
- (instancetype)initWithStride:(NSUInteger)stride;
@property NSUInteger stride;
@end

@interface MDLVertexDescriptor : NSObject <NSCopying>
- (instancetype)initWithVertexDescriptor:(MDLVertexDescriptor *)vertexDescriptor;
- (MDLVertexAttribute *)attributeNamed:(NSString *)name;
- (void)addOrReplaceAttribute:(MDLVertexAttribute *)attribute;
- (void)removeAttributeNamed:(NSString *)name;
@property (retain) NSMutableArray<MDLVertexAttribute *> *attributes;
@property (retain) NSMutableArray<MDLVertexBufferLayout *> *layouts;
- (void)reset;
- (void)setPackedStrides;
- (void)setPackedOffsets;
@end

@interface MDLVertexAttributeData : NSObject
@property (retain) MDLMeshBufferMap *map;
@property void *dataStart;
@property NSUInteger stride;
@property MDLVertexFormat format;
@property NSUInteger bufferSize;
@end

@protocol MDLComponent <NSObject>
@end

@protocol MDLObjectContainerComponent <MDLComponent, NSFastEnumeration>
- (void)addObject:(MDLObject *)object;
- (void)removeObject:(MDLObject *)object;
- (MDLObject *)objectAtIndexedSubscript:(NSUInteger)index;
@property (readonly) NSUInteger count;
@property (readonly, retain) NSArray<MDLObject *> *objects;
@end

@interface MDLObjectContainer : NSObject <MDLObjectContainerComponent>
@end

@protocol MDLTransformComponent <MDLComponent>
@property matrix_float4x4 matrix;
@end

@interface MDLTransform : NSObject <MDLTransformComponent>
- (instancetype)initWithMatrix:(matrix_float4x4)matrix;
@end

@interface MDLObject : NSObject
@property (copy) NSString *name;
@property (readonly) NSString *path;
@property (assign) MDLObject *parent;
@property (retain) id <MDLObjectContainerComponent> children;
@property (retain) id <MDLTransformComponent> transform;
@property BOOL hidden;
- (void)addChild:(MDLObject *)child;
- (MDLObject *)objectAtPath:(NSString *)path;
- (MDLAxisAlignedBoundingBox)boundingBoxAtTime:(NSTimeInterval)time;
@end

@interface MDLMaterial : NSObject
- (instancetype)initWithName:(NSString *)name scatteringFunction:(id)scatteringFunction;
@property (copy) NSString *name;
@property (readonly) NSUInteger count;
@end

@interface MDLSubmesh : NSObject
- (instancetype)initWithName:(NSString *)name indexBuffer:(id <MDLMeshBuffer>)indexBuffer indexCount:(NSUInteger)indexCount
                   indexType:(MDLIndexBitDepth)indexType geometryType:(MDLGeometryType)geometryType material:(MDLMaterial *)material;
- (instancetype)initWithIndexBuffer:(id <MDLMeshBuffer>)indexBuffer indexCount:(NSUInteger)indexCount
                          indexType:(MDLIndexBitDepth)indexType geometryType:(MDLGeometryType)geometryType material:(MDLMaterial *)material;
@property (readonly, retain) id <MDLMeshBuffer> indexBuffer;
- (id <MDLMeshBuffer>)indexBufferAsIndexType:(MDLIndexBitDepth)indexType;
@property (readonly) NSUInteger indexCount;
@property (readonly) MDLIndexBitDepth indexType;
@property (readonly) MDLGeometryType geometryType;
@property (retain) MDLMaterial *material;
@property (copy) NSString *name;
@end

@interface MDLMesh : MDLObject
- (instancetype)initWithBufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator;
- (instancetype)initWithVertexBuffer:(id <MDLMeshBuffer>)vertexBuffer vertexCount:(NSUInteger)vertexCount
                          descriptor:(MDLVertexDescriptor *)descriptor submeshes:(NSArray<MDLSubmesh *> *)submeshes;
- (instancetype)initWithVertexBuffers:(NSArray<id <MDLMeshBuffer>> *)vertexBuffers vertexCount:(NSUInteger)vertexCount
                           descriptor:(MDLVertexDescriptor *)descriptor submeshes:(NSArray<MDLSubmesh *> *)submeshes;
- (MDLVertexAttributeData *)vertexAttributeDataForAttributeNamed:(NSString *)name;
- (MDLVertexAttributeData *)vertexAttributeDataForAttributeNamed:(NSString *)name asFormat:(MDLVertexFormat)format;
@property (readonly) MDLAxisAlignedBoundingBox boundingBox;
@property (copy) MDLVertexDescriptor *vertexDescriptor;
@property NSUInteger vertexCount;
@property (retain) NSArray<id <MDLMeshBuffer>> *vertexBuffers;
@property (copy) NSMutableArray<MDLSubmesh *> *submeshes;
@property (readonly, retain) id <MDLMeshBufferAllocator> allocator;
- (void)addAttributeWithName:(NSString *)name format:(MDLVertexFormat)format;
- (void)addNormalsWithAttributeNamed:(NSString *)attributeName creaseThreshold:(float)creaseThreshold;
- (void)addTangentBasisForTextureCoordinateAttributeNamed:(NSString *)textureCoordinateAttributeName
                                    tangentAttributeNamed:(NSString *)tangentAttributeName
                                  bitangentAttributeNamed:(NSString *)bitangentAttributeName;
- (void)addTangentBasisForTextureCoordinateAttributeNamed:(NSString *)textureCoordinateAttributeName
                                     normalAttributeNamed:(NSString *)normalAttributeName
                                    tangentAttributeNamed:(NSString *)tangentAttributeName;
@end

@interface MDLAsset : NSObject <NSCopying, NSFastEnumeration>
- (instancetype)initWithURL:(NSURL *)URL;
- (instancetype)initWithURL:(NSURL *)URL vertexDescriptor:(MDLVertexDescriptor *)vertexDescriptor
            bufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator;
- (instancetype)initWithURL:(NSURL *)URL vertexDescriptor:(MDLVertexDescriptor *)vertexDescriptor
            bufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator preserveTopology:(BOOL)preserveTopology
                      error:(NSError **)error;
- (instancetype)initWithBufferAllocator:(id <MDLMeshBufferAllocator>)bufferAllocator;
+ (BOOL)canImportFileExtension:(NSString *)extension;
+ (BOOL)canExportFileExtension:(NSString *)extension;
- (void)addObject:(MDLObject *)object;
- (void)removeObject:(MDLObject *)object;
- (MDLObject *)objectAtIndex:(NSUInteger)index;
- (MDLObject *)objectAtIndexedSubscript:(NSUInteger)index;
- (MDLObject *)objectAtPath:(NSString *)path;
- (NSArray<MDLObject *> *)childObjectsOfClass:(Class)objectClass;
@property (readonly) NSUInteger count;
@property (readonly, retain) NSURL *URL;
@property (readonly, retain) id <MDLMeshBufferAllocator> bufferAllocator;
@property (readonly, retain) MDLVertexDescriptor *vertexDescriptor;
@property vector_float3 upAxis;
@property NSTimeInterval startTime;
@property NSTimeInterval endTime;
@property NSTimeInterval frameInterval;
@property (readonly) MDLAxisAlignedBoundingBox boundingBox;
@end

@interface MDLTexture : NSObject
+ (instancetype)textureNamed:(NSString *)name;
+ (instancetype)textureNamed:(NSString *)name bundle:(NSBundle *)bundleOrNil;
+ (instancetype)textureCubeWithImagesNamed:(NSArray<NSString *> *)names;
+ (instancetype)textureCubeWithImagesNamed:(NSArray<NSString *> *)names bundle:(NSBundle *)bundleOrNil;
- (instancetype)initWithData:(NSData *)pixelData topLeftOrigin:(BOOL)topLeftOrigin name:(NSString *)name
                  dimensions:(vector_int2)dimensions rowStride:(NSInteger)rowStride channelCount:(NSUInteger)channelCount
             channelEncoding:(MDLTextureChannelEncoding)channelEncoding isCube:(BOOL)isCube;
- (NSData *)texelDataWithTopLeftOrigin;
- (NSData *)texelDataWithBottomLeftOrigin;
- (NSData *)texelDataWithTopLeftOriginAtMipLevel:(NSInteger)level create:(BOOL)create;
- (NSData *)texelDataWithBottomLeftOriginAtMipLevel:(NSInteger)level create:(BOOL)create;
@property (readonly) vector_int2 dimensions;
@property (readonly) NSInteger rowStride;
@property (readonly) NSUInteger channelCount;
@property (readonly) NSUInteger mipLevelCount;
@property (readonly) MDLTextureChannelEncoding channelEncoding;
@property BOOL isCube;
@property BOOL hasAlphaValues;
@property (copy) NSString *name;
@end

#endif
