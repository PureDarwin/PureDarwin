/*
 * Apple's __exp10 / __exp10f (base-10 exponential): 10**x
 */
#include <math.h>

#ifndef M_LN10
#define M_LN10 2.30258509299404568402
#endif

double __exp10(double x)
{
    return exp(x * M_LN10);
}

float __exp10f(float x)
{
    return expf(x * (float)M_LN10);
}
