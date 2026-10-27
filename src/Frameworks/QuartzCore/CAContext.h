// CAContext.h - cross-process layer hosting in the shape of Apple's CAContext and CALayerHost, a layer's surface shown in another process's window.
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <QuartzCore/CALayer.h>
#include <stdint.h>

typedef uint32_t CAContextID;

@interface CAContext : NSObject

+ (instancetype)remoteContextWithOptions:(NSDictionary *)options;
+ (instancetype)remoteContext;

@property(retain) CALayer *layer;
// 0 until a hostable layer is set, or where the compositor cannot host across processes
@property(readonly) CAContextID contextId;

- (void)invalidate;

@end

@interface CALayerHost : CALayer

@property CAContextID contextId;

@end
