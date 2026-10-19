
#import <QuartzCore/CABase.h>
#import <CoreFoundation/CFCGTypes.h>

typedef struct {
    CGFloat m11, m12, m13, m14;
    CGFloat m21, m22, m23, m24;
    CGFloat m31, m32, m33, m34;
    CGFloat m41, m42, m43, m44;
} CATransform3D;

CA_EXPORT const CATransform3D CATransform3DIdentity;

CA_EXPORT bool CATransform3DIsIdentity(CATransform3D t);
CA_EXPORT bool CATransform3DEqualToTransform(CATransform3D a, CATransform3D b);
CA_EXPORT CATransform3D CATransform3DMakeTranslation(CGFloat tx, CGFloat ty, CGFloat tz);
CA_EXPORT CATransform3D CATransform3DMakeScale(CGFloat sx, CGFloat sy, CGFloat sz);
CA_EXPORT CATransform3D CATransform3DMakeRotation(CGFloat angle, CGFloat x, CGFloat y, CGFloat z);
CA_EXPORT CATransform3D CATransform3DTranslate(CATransform3D t, CGFloat tx, CGFloat ty, CGFloat tz);
CA_EXPORT CATransform3D CATransform3DScale(CATransform3D t, CGFloat sx, CGFloat sy, CGFloat sz);
CA_EXPORT CATransform3D CATransform3DRotate(CATransform3D t, CGFloat angle, CGFloat x, CGFloat y, CGFloat z);
CA_EXPORT CATransform3D CATransform3DConcat(CATransform3D a, CATransform3D b);
CA_EXPORT CATransform3D CATransform3DInvert(CATransform3D t);
CA_EXPORT CATransform3D CATransform3DMakeAffineTransform(CGAffineTransform m);
CA_EXPORT bool CATransform3DIsAffine(CATransform3D t);
CA_EXPORT CGAffineTransform CATransform3DGetAffineTransform(CATransform3D t);
