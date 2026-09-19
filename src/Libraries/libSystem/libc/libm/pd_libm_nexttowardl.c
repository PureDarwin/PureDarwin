/*
 * nexttowardl(3). openlibm aliases it to nextafterl with __strong_reference,
 * which emits nothing on Mach-O; for long double the two are identical.
 */
#include <float.h>
#include <math.h>

long double nexttowardl(long double x, long double y)
{
#if LDBL_MANT_DIG == DBL_MANT_DIG
    // arm64: long double is double, and openlibm builds no *l functions
    return nextafter((double)x, (double)y);
#else
    return nextafterl(x, y);
#endif
}
