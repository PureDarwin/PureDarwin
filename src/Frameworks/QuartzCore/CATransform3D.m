#import <QuartzCore/CATransform3D.h>

const CATransform3D CATransform3DIdentity = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
#include <math.h>
#include <string.h>

bool CATransform3DIsIdentity(CATransform3D t) {
    return CATransform3DEqualToTransform(t, CATransform3DIdentity);
}

bool CATransform3DEqualToTransform(CATransform3D a, CATransform3D b) {
    return memcmp(&a, &b, sizeof(a)) == 0;
}

CATransform3D CATransform3DMakeTranslation(CGFloat tx, CGFloat ty, CGFloat tz) {
    CATransform3D t = CATransform3DIdentity;

    t.m41 = tx;
    t.m42 = ty;
    t.m43 = tz;
    return t;
}

CATransform3D CATransform3DMakeScale(CGFloat sx, CGFloat sy, CGFloat sz) {
    CATransform3D t = CATransform3DIdentity;

    t.m11 = sx;
    t.m22 = sy;
    t.m33 = sz;
    return t;
}

// row-vector convention, as Core Animation uses: p' = p * M
CATransform3D CATransform3DMakeRotation(CGFloat angle, CGFloat x, CGFloat y, CGFloat z) {
    CGFloat length = sqrt(x * x + y * y + z * z);
    CGFloat c = cos(angle), s = sin(angle), ic = 1 - c;
    CATransform3D t = CATransform3DIdentity;

    if (length == 0) return t;
    x /= length;
    y /= length;
    z /= length;
    t.m11 = x * x * ic + c;     t.m12 = x * y * ic + z * s; t.m13 = x * z * ic - y * s;
    t.m21 = y * x * ic - z * s; t.m22 = y * y * ic + c;     t.m23 = y * z * ic + x * s;
    t.m31 = z * x * ic + y * s; t.m32 = z * y * ic - x * s; t.m33 = z * z * ic + c;
    return t;
}

CATransform3D CATransform3DConcat(CATransform3D a, CATransform3D b) {
    const CGFloat *x = &a.m11, *y = &b.m11;
    CATransform3D r;
    CGFloat *o = &r.m11;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            o[i * 4 + j] = x[i * 4] * y[j] + x[i * 4 + 1] * y[4 + j] + x[i * 4 + 2] * y[8 + j] + x[i * 4 + 3] * y[12 + j];
        }
    }
    return r;
}

CATransform3D CATransform3DTranslate(CATransform3D t, CGFloat tx, CGFloat ty, CGFloat tz) {
    return CATransform3DConcat(CATransform3DMakeTranslation(tx, ty, tz), t);
}

CATransform3D CATransform3DScale(CATransform3D t, CGFloat sx, CGFloat sy, CGFloat sz) {
    return CATransform3DConcat(CATransform3DMakeScale(sx, sy, sz), t);
}

CATransform3D CATransform3DRotate(CATransform3D t, CGFloat angle, CGFloat x, CGFloat y, CGFloat z) {
    return CATransform3DConcat(CATransform3DMakeRotation(angle, x, y, z), t);
}

// Gauss-Jordan with partial pivoting, a singular matrix comes back unchanged as on Apple
CATransform3D CATransform3DInvert(CATransform3D t) {
    double a[4][8];
    const CGFloat *m = &t.m11;
    CATransform3D r;
    CGFloat *o = &r.m11;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            a[i][j] = m[i * 4 + j];
            a[i][j + 4] = i == j;
        }
    }
    for (int col = 0; col < 4; col++) {
        int pivot = col;

        for (int i = col + 1; i < 4; i++)
            if (fabs(a[i][col]) > fabs(a[pivot][col])) pivot = i;
        if (fabs(a[pivot][col]) < 1e-12) return t;
        if (pivot != col) {
            for (int j = 0; j < 8; j++) {
                double tmp = a[col][j];

                a[col][j] = a[pivot][j];
                a[pivot][j] = tmp;
            }
        }
        double d = a[col][col];

        for (int j = 0; j < 8; j++) a[col][j] /= d;
        for (int i = 0; i < 4; i++) {
            if (i == col) continue;
            double f = a[i][col];

            for (int j = 0; j < 8; j++) a[i][j] -= f * a[col][j];
        }
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            o[i * 4 + j] = a[i][j + 4];
    return r;
}

CATransform3D CATransform3DMakeAffineTransform(CGAffineTransform m) {
    CATransform3D t = CATransform3DIdentity;

    t.m11 = m.a;
    t.m12 = m.b;
    t.m21 = m.c;
    t.m22 = m.d;
    t.m41 = m.tx;
    t.m42 = m.ty;
    return t;
}

bool CATransform3DIsAffine(CATransform3D t) {
    return t.m13 == 0 && t.m14 == 0 && t.m23 == 0 && t.m24 == 0 && t.m31 == 0 && t.m32 == 0 &&
           t.m33 == 1 && t.m34 == 0 && t.m43 == 0 && t.m44 == 1;
}

CGAffineTransform CATransform3DGetAffineTransform(CATransform3D t) {
    CGAffineTransform m = { t.m11, t.m12, t.m21, t.m22, t.m41, t.m42 };

    return m;
}
