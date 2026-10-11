// MetalPerformanceShaders.h - image kernels on Metal
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef MPS_METALPERFORMANCESHADERS_H
#define MPS_METALPERFORMANCESHADERS_H

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

typedef NS_ENUM(NSUInteger, MPSImageEdgeMode) {
    MPSImageEdgeModeZero = 0,
    MPSImageEdgeModeClamp = 1,
};

typedef NS_OPTIONS(NSUInteger, MPSKernelOptions) {
    MPSKernelOptionsNone = 0,
    MPSKernelOptionsSkipAPIValidation = 1U << 0,
    MPSKernelOptionsAllowReducedPrecision = 1U << 1,
};

typedef struct {
    NSInteger x, y, z;
} MPSOffset;

typedef struct {
    MTLOrigin origin;
    MTLSize size;
} MPSRegion;

extern const MTLRegion MPSRectNoClip;

typedef id<MTLTexture> (^MPSCopyAllocator)(id kernel, id<MTLCommandBuffer> commandBuffer, id<MTLTexture> sourceTexture);

BOOL MPSSupportsMTLDevice(id<MTLDevice> device);

@interface MPSKernel : NSObject <NSCopying>
- (instancetype)initWithDevice:(id<MTLDevice>)device;
@property (readonly, retain) id<MTLDevice> device;
@property MPSKernelOptions options;
@property (copy) NSString *label;
@end

@interface MPSUnaryImageKernel : MPSKernel
@property MPSOffset offset;
@property MTLRegion clipRect;
@property MPSImageEdgeMode edgeMode;
- (BOOL)encodeToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer inPlaceTexture:(id<MTLTexture> *)texture
        fallbackCopyAllocator:(MPSCopyAllocator)copyAllocator;
- (void)encodeToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer sourceTexture:(id<MTLTexture>)sourceTexture
           destinationTexture:(id<MTLTexture>)destinationTexture;
@end

@interface MPSImageGaussianBlur : MPSUnaryImageKernel
- (instancetype)initWithDevice:(id<MTLDevice>)device sigma:(float)sigma;
@property (readonly) float sigma;
@end

#endif
