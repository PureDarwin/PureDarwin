// CAContext-Private.h - how AppKit places a CALayerHost, the same calls a hosted CAMetalLayer takes.
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <QuartzCore/CAContext.h>

@interface CALayerHost (PDPrivate)
- (BOOL)_pdAttachToWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size;
- (void)_pdDetach;
- (void)_pdSetViewSize:(CGSize)size;
- (BOOL)_pdIsAttached;
- (void)setWindowOrigin:(CGPoint)origin;
@end
