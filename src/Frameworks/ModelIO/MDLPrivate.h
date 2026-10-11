// MDLPrivate.h - what the ModelIO sources share with each other
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import "ModelIO.h"

NSUInteger MDLVertexFormatComponents(MDLVertexFormat format);
NSUInteger MDLVertexFormatSize(MDLVertexFormat format);
void MDLReadVertexElement(const void *source, MDLVertexFormat format, float out[4]);
void MDLWriteVertexElement(void *destination, MDLVertexFormat format, const float in[4]);
uint32_t MDLReadIndex(const void *indices, MDLIndexBitDepth type, NSUInteger i);

@interface MDLMeshBufferData (Private)
- (void)_setAllocator:(id <MDLMeshBufferAllocator>)allocator;
@end

@interface MDLMeshBufferZoneDefault (Private)
- (instancetype)initWithCapacity:(NSUInteger)capacity allocator:(id <MDLMeshBufferAllocator>)allocator;
@end

@interface MDLObject (Private)
- (void)_setPath:(NSString *)path;
@end
