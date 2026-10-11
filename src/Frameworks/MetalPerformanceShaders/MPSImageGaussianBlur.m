// MPSImageGaussianBlur.m - a Gaussian blur as two passes of a separable compute kernel
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import "MetalPerformanceShaders.h"
#include <math.h>

const MTLRegion MPSRectNoClip = { { 0, 0, 0 }, { NSUIntegerMax, NSUIntegerMax, NSUIntegerMax } };

// the layout MPSKernels.metal reads its parameters in
typedef struct {
    int32_t radius;
    int32_t horizontal;
    int32_t edgeMode;
    int32_t padding;
    int32_t offset[2];
} MPSBlurParams;

BOOL MPSSupportsMTLDevice(id<MTLDevice> device) {
    return device != nil;
}

@implementation MPSKernel {
    id<MTLDevice> _device;
    MPSKernelOptions _options;
    NSString *_label;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device {
    if ((self = [super init]) != nil)
        _device = [device retain];
    return self;
}

- (void)dealloc {
    [_device release];
    [_label release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    MPSKernel *copy = [[[self class] alloc] initWithDevice:_device];
    copy->_options = _options;
    copy->_label = [_label copy];
    return copy;
}

- (id<MTLDevice>)device { return _device; }
- (MPSKernelOptions)options { return _options; }
- (void)setOptions:(MPSKernelOptions)options { _options = options; }
- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label { NSString *old = _label; _label = [label copy]; [old release]; }

@end

@implementation MPSUnaryImageKernel {
    MPSOffset _offset;
    MTLRegion _clipRect;
    MPSImageEdgeMode _edgeMode;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device {
    if ((self = [super initWithDevice:device]) != nil)
        _clipRect = MPSRectNoClip;
    return self;
}

- (MPSOffset)offset { return _offset; }
- (void)setOffset:(MPSOffset)offset { _offset = offset; }
- (MTLRegion)clipRect { return _clipRect; }
- (void)setClipRect:(MTLRegion)clipRect { _clipRect = clipRect; }
- (MPSImageEdgeMode)edgeMode { return _edgeMode; }
- (void)setEdgeMode:(MPSImageEdgeMode)edgeMode { _edgeMode = edgeMode; }

- (void)encodeToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer sourceTexture:(id<MTLTexture>)sourceTexture
           destinationTexture:(id<MTLTexture>)destinationTexture {
    [NSException raise:NSInternalInconsistencyException format:@"%@ does not implement %@", [self class], NSStringFromSelector(_cmd)];
}

// these kernels read neighbouring texels, so the result goes to a new texture and replaces the old one
- (BOOL)encodeToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer inPlaceTexture:(id<MTLTexture> *)texture
        fallbackCopyAllocator:(MPSCopyAllocator)copyAllocator {
    if (texture == NULL || *texture == nil || copyAllocator == nil)
        return NO;

    id<MTLTexture> destination = copyAllocator(self, commandBuffer, *texture);
    if (destination == nil)
        return NO;
    [self encodeToCommandBuffer:commandBuffer sourceTexture:*texture destinationTexture:destination];
    [*texture release];
    *texture = destination;
    return YES;
}

@end

@implementation MPSImageGaussianBlur {
    float _sigma;
    int _radius;
    id<MTLBuffer> _weights;
    id<MTLComputePipelineState> _pipeline;
    id<MTLTexture> _intermediate;
}

// the kernels ship precompiled next to the framework binary
static id<MTLComputePipelineState> blurPipeline(id<MTLDevice> device) {
    NSBundle *framework = [NSBundle bundleWithPath:@"/System/Library/Frameworks/MetalPerformanceShaders.framework"];
    NSString *path = [framework pathForResource:@"MPSKernels" ofType:@"metallib"];
    NSError *error = nil;
    id<MTLLibrary> library = path != nil ? [device newLibraryWithURL:[NSURL fileURLWithPath:path] error:&error] : nil;
    id<MTLFunction> function = [library newFunctionWithName:@"mps_separable_blur"];
    id<MTLComputePipelineState> pipeline = function != nil ? [device newComputePipelineStateWithFunction:function error:&error] : nil;

    if (pipeline == nil)
        NSLog(@"MPSImageGaussianBlur: no blur kernel (%@): %@", path, error);
    [function release];
    [library release];
    return pipeline;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device sigma:(float)sigma {
    if ((self = [super initWithDevice:device]) == nil)
        return nil;

    _sigma = sigma > 0 ? sigma : 0;
    _radius = (int)ceilf(3.0f * _sigma);
    int taps = 2 * _radius + 1;
    float *weights = malloc(sizeof(float) * taps);
    float total = 0;

    for (int i = -_radius; i <= _radius; i++) {
        weights[i + _radius] = _sigma > 0 ? expf(-(float)(i * i) / (2.0f * _sigma * _sigma)) : 1.0f;
        total += weights[i + _radius];
    }
    for (int i = 0; i < taps; i++)
        weights[i] /= total;
    _weights = [device newBufferWithBytes:weights length:sizeof(float) * taps options:MTLResourceStorageModeShared];
    free(weights);
    _pipeline = blurPipeline(device);
    return self;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device {
    return [self initWithDevice:device sigma:1.0f];
}

- (void)dealloc {
    [_weights release];
    [_pipeline release];
    [_intermediate release];
    [super dealloc];
}

- (float)sigma {
    return _sigma;
}

- (id)copyWithZone:(NSZone *)zone {
    MPSImageGaussianBlur *copy = [[MPSImageGaussianBlur alloc] initWithDevice:self.device sigma:_sigma];
    copy.offset = self.offset;
    copy.clipRect = self.clipRect;
    copy.edgeMode = self.edgeMode;
    return copy;
}

// the horizontal pass goes to a texture kept between frames, the vertical one to the destination
- (id<MTLTexture>)intermediateLike:(id<MTLTexture>)texture {
    if (_intermediate != nil && _intermediate.width == texture.width && _intermediate.height == texture.height &&
        _intermediate.pixelFormat == texture.pixelFormat)
        return _intermediate;

    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:texture.pixelFormat
                                                                                          width:texture.width
                                                                                         height:texture.height
                                                                                      mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    descriptor.storageMode = MTLStorageModePrivate;
    [_intermediate release];
    _intermediate = [self.device newTextureWithDescriptor:descriptor];
    return _intermediate;
}

- (void)encodePass:(id<MTLComputeCommandEncoder>)encoder source:(id<MTLTexture>)source destination:(id<MTLTexture>)destination
        horizontal:(BOOL)horizontal offset:(MPSOffset)offset {
    MPSBlurParams params = { _radius, horizontal, self.edgeMode == MPSImageEdgeModeClamp, 0, { (int32_t)offset.x, (int32_t)offset.y } };
    NSUInteger width = _pipeline.threadExecutionWidth ?: 8;
    NSUInteger height = MAX(1, (_pipeline.maxTotalThreadsPerThreadgroup ?: 64) / width);
    MTLSize group = MTLSizeMake(width, height, 1);
    MTLSize groups = MTLSizeMake((destination.width + width - 1) / width, (destination.height + height - 1) / height, 1);

    [encoder setTexture:source atIndex:0];
    [encoder setTexture:destination atIndex:1];
    [encoder setBuffer:_weights offset:0 atIndex:0];
    [encoder setBytes:&params length:sizeof(params) atIndex:1];
    [encoder dispatchThreadgroups:groups threadsPerThreadgroup:group];
}

- (void)encodeToCommandBuffer:(id<MTLCommandBuffer>)commandBuffer sourceTexture:(id<MTLTexture>)sourceTexture
           destinationTexture:(id<MTLTexture>)destinationTexture {
    if (_pipeline == nil || sourceTexture == nil || destinationTexture == nil)
        return;

    id<MTLTexture> intermediate = [self intermediateLike:sourceTexture];
    MPSOffset none = { 0, 0, 0 };

    // one encoder per pass, so the second reads what the first has finished writing
    id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
    [encoder setComputePipelineState:_pipeline];
    [self encodePass:encoder source:sourceTexture destination:intermediate horizontal:YES offset:self.offset];
    [encoder endEncoding];

    encoder = [commandBuffer computeCommandEncoder];
    [encoder setComputePipelineState:_pipeline];
    [self encodePass:encoder source:intermediate destination:destinationTexture horizontal:NO offset:none];
    [encoder endEncoding];
}

@end
