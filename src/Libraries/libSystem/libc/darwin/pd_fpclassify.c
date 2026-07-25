/*
 * __fpclassifyf/__fpclassifyd/__fpclassifyl - real Apple's <math.h> fpclassify(x)
 * macro (see SDK usr/include/math.h) expands to a call to one of these Libm
 * entry points rather than a compiler builtin directly. We don't have Libm in
 * this tree; gdtoa's _hdtoa.c/_ldtoa.c (hex-float / long-double formatting,
 * reachable only via printf's rarely-used %a/%Lf specifiers) need them to
 * link. This is the real, standard, correct implementation -- just calling
 * clang's __builtin_fpclassify directly instead of going through the SDK
 * macro that would otherwise recurse back into these same functions.
 */

#include <math.h>

int
__fpclassifyf(float f)
{
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, f);
}

int
__fpclassifyd(double d)
{
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, d);
}

int
__fpclassifyl(long double d)
{
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, d);
}
