// MTKModel.h - ModelIO meshes in Metal buffers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_METALKIT_MTKMODEL_H
#define PD_METALKIT_MTKMODEL_H

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <ModelIO/ModelIO.h>
#import <MetalKit/MTKTextureLoader.h>

extern NSString * const MTKModelErrorDomain;
extern NSString * const MTKModelErrorKey;

@interface MTKMeshBufferAllocator : NSObject <MDLMeshBufferAllocator>
- (instancetype)initWithDevice:(id<MTLDevice>)device;
@property (nonatomic, readonly) id<MTLDevice> device;
@end

@interface MTKMeshBuffer : NSObject <MDLMeshBuffer>
@property (nonatomic, readonly) NSUInteger length;
@property (nonatomic, readonly) MTKMeshBufferAllocator *allocator;
@property (nonatomic, readonly) id<MDLMeshBufferZone> zone;
@property (nonatomic, readonly) id<MTLBuffer> buffer;
@property (nonatomic, readonly) NSUInteger offset;
@property (nonatomic, readonly) MDLMeshBufferType type;
@end

@class MTKMesh;

@interface MTKSubmesh : NSObject
@property (nonatomic, readonly) MTLPrimitiveType primitiveType;
@property (nonatomic, readonly) MTLIndexType indexType;
@property (nonatomic, readonly) MTKMeshBuffer *indexBuffer;
@property (nonatomic, readonly) NSUInteger indexCount;
@property (nonatomic, readonly, assign) MTKMesh *mesh;
@property (nonatomic, copy) NSString *name;
@end

@interface MTKMesh : NSObject
- (instancetype)initWithMesh:(MDLMesh *)mesh device:(id<MTLDevice>)device error:(NSError **)error;
+ (NSArray<MTKMesh *> *)newMeshesFromAsset:(MDLAsset *)asset device:(id<MTLDevice>)device
                              sourceMeshes:(NSArray<MDLMesh *> **)sourceMeshes error:(NSError **)error;
@property (nonatomic, readonly) NSArray<MTKMeshBuffer *> *vertexBuffers;
@property (nonatomic, readonly) MDLVertexDescriptor *vertexDescriptor;
@property (nonatomic, readonly) NSArray<MTKSubmesh *> *submeshes;
@property (nonatomic, readonly) NSUInteger vertexCount;
@property (nonatomic, copy) NSString *name;
@end

#ifdef __cplusplus
extern "C" {
#endif
MDLVertexDescriptor *MTKModelIOVertexDescriptorFromMetal(MTLVertexDescriptor *metalDescriptor);
MDLVertexDescriptor *MTKModelIOVertexDescriptorFromMetalWithError(MTLVertexDescriptor *metalDescriptor, NSError **error);
MTLVertexDescriptor *MTKMetalVertexDescriptorFromModelIO(MDLVertexDescriptor *modelIODescriptor);
MTLVertexDescriptor *MTKMetalVertexDescriptorFromModelIOWithError(MDLVertexDescriptor *modelIODescriptor, NSError **error);
MDLVertexFormat MTKModelIOVertexFormatFromMetal(MTLVertexFormat vertexFormat);
MTLVertexFormat MTKMetalVertexFormatFromModelIO(MDLVertexFormat vertexFormat);
#ifdef __cplusplus
}
#endif

@interface MTKTextureLoader (ModelIO)
- (id<MTLTexture>)newTextureWithMDLTexture:(MDLTexture *)texture
                                   options:(NSDictionary<MTKTextureLoaderOption, id> *)options
                                     error:(NSError **)error;
@end

#endif
