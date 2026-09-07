/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSAffineTransform_h
#define NSAffineTransform_h

#import <Foundation/NSGeometry.h>
#import <Foundation/NSObject.h>

typedef struct {
    CGFloat m11;
    CGFloat m12;
    CGFloat m21;
    CGFloat m22;
    CGFloat tX;
    CGFloat tY;
} NSAffineTransformStruct;

@interface NSAffineTransform : NSObject <NSCopying> {
    NSAffineTransformStruct _matrix;
}

+ (NSAffineTransform *)transform;

- (instancetype)initWithTransform:(NSAffineTransform *)transform;
- (NSAffineTransformStruct)transformStruct;
- (void)setTransformStruct:(NSAffineTransformStruct)matrix;
- (void)invert;
- (void)appendTransform:(NSAffineTransform *)transform;
- (void)prependTransform:(NSAffineTransform *)transform;
- (void)translateXBy:(CGFloat)x yBy:(CGFloat)y;
- (NSPoint)transformPoint:(NSPoint)point;
- (NSSize)transformSize:(NSSize)size;
- (void)rotateByDegrees:(CGFloat)angle;
- (void)rotateByRadians:(CGFloat)angle;
- (void)scaleBy:(CGFloat)scale;
- (void)scaleXBy:(CGFloat)x yBy:(CGFloat)y;

@end

#endif /* NSAffineTransform_h */
