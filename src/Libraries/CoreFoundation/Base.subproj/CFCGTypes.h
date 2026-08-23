/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/* On Darwin the geometry types are owned by CoreFoundation, not CoreGraphics:
 * CGFloat/CGPoint/CGSize/CGRect are declared here so Foundation can typedef
 * NSPoint and friends onto them without depending on CoreGraphics. */

#if !defined(__COREFOUNDATION_CFCGTYPES__)
#define __COREFOUNDATION_CFCGTYPES__ 1

#include <CoreFoundation/CFBase.h>
#include <stdint.h>

CF_EXTERN_C_BEGIN

#ifndef CGFLOAT_DEFINED
    #define CGFLOAT_DEFINED 1
    #if defined(__LP64__) && __LP64__
        typedef double CGFloat;
        #define CGFLOAT_MIN         2.2250738585072014e-308
        #define CGFLOAT_MAX         1.7976931348623157e+308
        #define CGFLOAT_EPSILON     2.2204460492503131e-16
        #define CGFLOAT_IS_DOUBLE   1
    #else
        typedef float CGFloat;
        #define CGFLOAT_MIN         1.17549435e-38F
        #define CGFLOAT_MAX         3.40282347e+38F
        #define CGFLOAT_EPSILON     1.19209290e-7F
        #define CGFLOAT_IS_DOUBLE   0
    #endif
    #define CGFLOAT_TYPE_IS_DOUBLE  CGFLOAT_IS_DOUBLE
#endif

struct CGPoint {
    CGFloat x;
    CGFloat y;
};
typedef struct CGPoint CGPoint;

struct CGSize {
    CGFloat width;
    CGFloat height;
};
typedef struct CGSize CGSize;

struct CGVector {
    CGFloat dx;
    CGFloat dy;
};
typedef struct CGVector CGVector;

struct CGRect {
    CGPoint origin;
    CGSize size;
};
typedef struct CGRect CGRect;

typedef CF_ENUM(uint32_t, CGRectEdge) {
    CGRectMinXEdge = 0,
    CGRectMinYEdge = 1,
    CGRectMaxXEdge = 2,
    CGRectMaxYEdge = 3,
};

struct CGAffineTransform {
    CGFloat a;
    CGFloat b;
    CGFloat c;
    CGFloat d;
    CGFloat tx;
    CGFloat ty;
};
typedef struct CGAffineTransform CGAffineTransform;

CF_EXTERN_C_END

#endif /* ! __COREFOUNDATION_CFCGTYPES__ */
