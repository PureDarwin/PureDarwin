/*
 * Apple's __sincos*_stret entry points: on x86_64 the "stret" name is a
 * historical leftover from i386 (where returning a small struct needed an
 * explicit calling-convention marker) - on x86_64 a struct of two doubles
 * (or two floats) already returns in xmm0:xmm1 under the standard SysV ABI,
 * so a plain C function returning these structs matches the ABI callers
 * (Foundation/CoreGraphics) expect. openlibm doesn't export these Apple-only
 * names, or sincospi/sincospif at all, so they're implemented here on top of
 * its sincos/sincosf plus a plain sin(M_PI*x)/cos(M_PI*x) for the pi variants.
 */
#include <math.h>

extern void sincos(double, double *, double *);
extern void sincosf(float, float *, float *);

struct __double2 __sincos_stret(double x)
{
    struct __double2 r;
    sincos(x, &r.__sinval, &r.__cosval);
    return r;
}

struct __float2 __sincosf_stret(float x)
{
    struct __float2 r;
    sincosf(x, &r.__sinval, &r.__cosval);
    return r;
}

struct __double2 __sincospi_stret(double x)
{
    struct __double2 r;
    sincos(x * M_PI, &r.__sinval, &r.__cosval);
    return r;
}

struct __float2 __sincospif_stret(float x)
{
    struct __float2 r;
    sincosf(x * (float)M_PI, &r.__sinval, &r.__cosval);
    return r;
}
