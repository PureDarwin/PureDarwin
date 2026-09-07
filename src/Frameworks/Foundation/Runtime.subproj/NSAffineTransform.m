/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSAffineTransform.h>
#import <Foundation/NSException.h>

#include <math.h>

static const NSAffineTransformStruct identity = { 1, 0, 0, 1, 0, 0 };

static NSAffineTransformStruct multiply(NSAffineTransformStruct first,
    NSAffineTransformStruct second) {
    NSAffineTransformStruct result;

    result.m11 = second.m11 * first.m11 + second.m12 * first.m21;
    result.m12 = second.m11 * first.m12 + second.m12 * first.m22;
    result.m21 = second.m21 * first.m11 + second.m22 * first.m21;
    result.m22 = second.m21 * first.m12 + second.m22 * first.m22;
    result.tX = second.tX * first.m11 + second.tY * first.m21 + first.tX;
    result.tY = second.tX * first.m12 + second.tY * first.m22 + first.tY;
    return result;
}

@implementation NSAffineTransform

- (instancetype)init {
    self = [super init];
    if (self != nil)
        _matrix = identity;
    return self;
}

- (instancetype)initWithTransform:(NSAffineTransform *)transform {
    self = [super init];
    if (self != nil)
        _matrix = transform != nil ? [transform transformStruct] : identity;
    return self;
}

+ (NSAffineTransform *)transform {
    return [[[self alloc] init] autorelease];
}

- (id)copyWithZone:(NSZone *)zone {
    return [[[self class] allocWithZone:zone] initWithTransform:self];
}

- (NSAffineTransformStruct)transformStruct {
    return _matrix;
}

- (void)setTransformStruct:(NSAffineTransformStruct)matrix {
    _matrix = matrix;
}

- (void)invert {
    CGFloat determinant = _matrix.m11 * _matrix.m22 - _matrix.m21 * _matrix.m12;
    if (determinant == 0.0) {
        [NSException raise:NSGenericException
                    format:@"NSAffineTransform has no inverse"];
        return;
    }

    NSAffineTransformStruct inverse;
    inverse.m11 = _matrix.m22 / determinant;
    inverse.m12 = -_matrix.m12 / determinant;
    inverse.m21 = -_matrix.m21 / determinant;
    inverse.m22 = _matrix.m11 / determinant;
    inverse.tX = (-_matrix.m22 * _matrix.tX + _matrix.m21 * _matrix.tY) / determinant;
    inverse.tY = (_matrix.m12 * _matrix.tX - _matrix.m11 * _matrix.tY) / determinant;
    _matrix = inverse;
}

- (void)appendTransform:(NSAffineTransform *)transform {
    if (transform != nil)
        _matrix = multiply([transform transformStruct], _matrix);
}

- (void)prependTransform:(NSAffineTransform *)transform {
    if (transform != nil)
        _matrix = multiply(_matrix, [transform transformStruct]);
}

- (void)translateXBy:(CGFloat)x yBy:(CGFloat)y {
    NSAffineTransformStruct translation = { 1, 0, 0, 1, x, y };
    _matrix = multiply(_matrix, translation);
}

- (NSPoint)transformPoint:(NSPoint)point {
    return NSMakePoint(_matrix.m11 * point.x + _matrix.m21 * point.y + _matrix.tX,
        _matrix.m12 * point.x + _matrix.m22 * point.y + _matrix.tY);
}

- (NSSize)transformSize:(NSSize)size {
    return NSMakeSize(_matrix.m11 * size.width + _matrix.m21 * size.height,
        _matrix.m12 * size.width + _matrix.m22 * size.height);
}

- (void)rotateByDegrees:(CGFloat)angle {
    [self rotateByRadians:M_PI * angle / 180.0];
}

- (void)rotateByRadians:(CGFloat)angle {
    NSAffineTransformStruct rotation = {
        cos(angle), sin(angle), -sin(angle), cos(angle), 0, 0
    };
    _matrix = multiply(_matrix, rotation);
}

- (void)scaleBy:(CGFloat)scale {
    [self scaleXBy:scale yBy:scale];
}

- (void)scaleXBy:(CGFloat)x yBy:(CGFloat)y {
    NSAffineTransformStruct scaling = { x, 0, 0, y, 0, 0 };
    _matrix = multiply(_matrix, scaling);
}

@end
