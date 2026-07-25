/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/*
 * from: @(#)fdlibm.h 5.1 93/09/24
 * $FreeBSD: src/lib/msun/src/openlibm.h,v 1.82 2011/11/12 19:55:48 theraven Exp $
 */

#ifdef OPENLIBM_USE_HOST_MATH_H
#include <math.h>
#else /* !OPENLIBM_USE_HOST_MATH_H */

#ifndef OPENLIBM_MATH_H
#define	OPENLIBM_MATH_H

#if (defined(_WIN32) || defined (_MSC_VER)) && !defined(__WIN32__)
    #define __WIN32__
#endif

#ifndef __arm__
#define LONG_DOUBLE
#endif

#ifndef __pure2
#define __pure2
#endif

#ifdef _WIN32
# ifdef IMPORT_EXPORTS
#  define OLM_DLLEXPORT __declspec(dllimport)
# else
#  define OLM_DLLEXPORT __declspec(dllexport)
# endif
#else
#define OLM_DLLEXPORT __attribute__ ((visibility("default")))
#endif

/*
 * ANSI/POSIX
 */
extern const union __infinity_un {
	unsigned char	__uc[8];
	double		__ud;
} __infinity;

extern const union __nan_un {
	unsigned char	__uc[sizeof(float)];
	float		__uf;
} __nan;

/* VBS
#if __GNUC_PREREQ__(3, 3) || (defined(__INTEL_COMPILER) && __INTEL_COMPILER >= 800)
#define	__MATH_BUILTIN_CONSTANTS
#endif

#if __GNUC_PREREQ__(3, 0) && !defined(__INTEL_COMPILER)
#define	__MATH_BUILTIN_RELOPS
#endif
*/

//VBS begin
#define __MATH_BUILTIN_CONSTANTS
#define	__MATH_BUILTIN_RELOPS
#ifndef __ISO_C_VISIBLE
#define __ISO_C_VISIBLE 1999
#endif
//VBS end

#ifdef __MATH_BUILTIN_CONSTANTS
#define	HUGE_VAL	__builtin_huge_val()
#else
#define	HUGE_VAL	(__infinity.__ud)
#endif

#if __ISO_C_VISIBLE >= 1999
#define	FP_ILOGB0	(-INT_MAX)
#define	FP_ILOGBNAN	INT_MAX

#ifdef __MATH_BUILTIN_CONSTANTS
#define	HUGE_VALF	__builtin_huge_valf()
#define	HUGE_VALL	__builtin_huge_vall()
#define	INFINITY	__builtin_inff()
#define	NAN		__builtin_nanf("")
#else
#define	HUGE_VALF	(float)HUGE_VAL
#define	HUGE_VALL	(long double)HUGE_VAL
#define	INFINITY	HUGE_VALF
#define	NAN		(__nan.__uf)
#endif /* __MATH_BUILTIN_CONSTANTS */

#define	MATH_ERRNO	1
#define	MATH_ERREXCEPT	2
#define	math_errhandling	MATH_ERREXCEPT

#define	FP_FAST_FMAF	1
#ifdef __ia64__
#define	FP_FAST_FMA	1
#define	FP_FAST_FMAL	1
#endif

/* Symbolic constants to classify floating point numbers. */
#define	FP_INFINITE	0x01
#define	FP_NAN		0x02
#define	FP_NORMAL	0x04
#define	FP_SUBNORMAL	0x08
#define	FP_ZERO		0x10
#define	fpclassify(x) \
    ((sizeof (x) == sizeof (float)) ? __fpclassifyf(x) \
    : (sizeof (x) == sizeof (double)) ? __fpclassifyd(x) \
    : __fpclassifyl(x))

#define	isfinite(x)					\
    ((sizeof (x) == sizeof (float)) ? __isfinitef(x)	\
    : (sizeof (x) == sizeof (double)) ? __isfinite(x)	\
    : __isfinitel(x))
#define	isinf(x)					\
    ((sizeof (x) == sizeof (float)) ? __isinff(x)	\
    : (sizeof (x) == sizeof (double)) ? isinf(x)	\
    : __isinfl(x))
#define	isnan(x)					\
    ((sizeof (x) == sizeof (float)) ? __isnanf(x)	\
    : (sizeof (x) == sizeof (double)) ? isnan(x)	\
    : __isnanl(x))
#define	isnormal(x)					\
    ((sizeof (x) == sizeof (float)) ? __isnormalf(x)	\
    : (sizeof (x) == sizeof (double)) ? __isnormal(x)	\
    : __isnormall(x))

#ifdef __MATH_BUILTIN_RELOPS
#define	isgreater(x, y)		__builtin_isgreater((x), (y))
#define	isgreaterequal(x, y)	__builtin_isgreaterequal((x), (y))
#define	isless(x, y)		__builtin_isless((x), (y))
#define	islessequal(x, y)	__builtin_islessequal((x), (y))
#define	islessgreater(x, y)	__builtin_islessgreater((x), (y))
#define	isunordered(x, y)	__builtin_isunordered((x), (y))
#else
#define	isgreater(x, y)		(!isunordered((x), (y)) && (x) > (y))
#define	isgreaterequal(x, y)	(!isunordered((x), (y)) && (x) >= (y))
#define	isless(x, y)		(!isunordered((x), (y)) && (x) < (y))
#define	islessequal(x, y)	(!isunordered((x), (y)) && (x) <= (y))
#define	islessgreater(x, y)	(!isunordered((x), (y)) && \
					((x) > (y) || (y) > (x)))
#define	isunordered(x, y)	(isnan(x) || isnan(y))
#endif /* __MATH_BUILTIN_RELOPS */

#define	signbit(x)					\
    ((sizeof (x) == sizeof (float)) ? __signbitf(x)	\
    : (sizeof (x) == sizeof (double)) ? __signbit(x)	\
    : __signbitl(x))

//VBS
//typedef	__double_t	double_t;
//typedef	__float_t	float_t;
#endif /* __ISO_C_VISIBLE >= 1999 */

/*
 * XOPEN/SVID
 */
#if __BSD_VISIBLE || __XSI_VISIBLE
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#define	MAXFLOAT	((float)3.40282346638528860e+38)

#ifndef OPENLIBM_ONLY_THREAD_SAFE
extern int signgam;
#endif
#endif /* __BSD_VISIBLE || __XSI_VISIBLE */

#if __BSD_VISIBLE
#if 0
/* Old value from 4.4BSD-Lite openlibm.h; this is probably better. */
#define	HUGE		HUGE_VAL
#else
#define	HUGE		MAXFLOAT
#endif
#endif /* __BSD_VISIBLE */

/*
 * Most of these functions depend on the rounding mode and have the side
 * effect of raising floating-point exceptions, so they are not declared
 * as __pure2.  In C99, FENV_ACCESS affects the purity of these functions.
 */

#if defined(__cplusplus)
extern "C" {
#endif
/* Symbol present when OpenLibm is used. */
int isopenlibm(void);

/*
 * ANSI/POSIX
 */
OLM_DLLEXPORT int	__fpclassifyd(double) __pure2;
OLM_DLLEXPORT int	__fpclassifyf(float) __pure2;
OLM_DLLEXPORT int	__fpclassifyl(long double) __pure2;
OLM_DLLEXPORT int	__isfinitef(float) __pure2;
OLM_DLLEXPORT int	__isfinite(double) __pure2;
OLM_DLLEXPORT int	__isfinitel(long double) __pure2;
OLM_DLLEXPORT int	__isinff(float) __pure2;
OLM_DLLEXPORT int	__isinfl(long double) __pure2;
OLM_DLLEXPORT int	__isnanf(float) __pure2;
OLM_DLLEXPORT int	__isnanl(long double) __pure2;
OLM_DLLEXPORT int	__isnormalf(float) __pure2;
OLM_DLLEXPORT int	__isnormal(double) __pure2;
OLM_DLLEXPORT int	__isnormall(long double) __pure2;
OLM_DLLEXPORT int	__signbit(double) __pure2;
OLM_DLLEXPORT int	__signbitf(float) __pure2;
OLM_DLLEXPORT int	__signbitl(long double) __pure2;

OLM_DLLEXPORT double	acos(double);
OLM_DLLEXPORT double	asin(double);
OLM_DLLEXPORT double	atan(double);
OLM_DLLEXPORT double	atan2(double, double);
OLM_DLLEXPORT double	cos(double);
OLM_DLLEXPORT double	sin(double);
OLM_DLLEXPORT double	tan(double);

OLM_DLLEXPORT double	cosh(double);
OLM_DLLEXPORT double	sinh(double);
OLM_DLLEXPORT double	tanh(double);

OLM_DLLEXPORT double	exp(double);
OLM_DLLEXPORT double	frexp(double, int *);	/* fundamentally !__pure2 */
OLM_DLLEXPORT double	ldexp(double, int);
OLM_DLLEXPORT double	log(double);
OLM_DLLEXPORT double	log10(double);
OLM_DLLEXPORT double	modf(double, double *);	/* fundamentally !__pure2 */

OLM_DLLEXPORT double	pow(double, double);
OLM_DLLEXPORT double	sqrt(double);

OLM_DLLEXPORT double	ceil(double);
OLM_DLLEXPORT double	fabs(double) __pure2;
OLM_DLLEXPORT double	floor(double);
OLM_DLLEXPORT double	fmod(double, double);

/*
 * These functions are not in C90.
 */
#if __BSD_VISIBLE || __ISO_C_VISIBLE >= 1999 || __XSI_VISIBLE
OLM_DLLEXPORT double	acosh(double);
OLM_DLLEXPORT double	asinh(double);
OLM_DLLEXPORT double	atanh(double);
OLM_DLLEXPORT double	cbrt(double);
OLM_DLLEXPORT double	erf(double);
OLM_DLLEXPORT double	erfc(double);
OLM_DLLEXPORT double	exp2(double);
OLM_DLLEXPORT double	expm1(double);
OLM_DLLEXPORT double	fma(double, double, double);
OLM_DLLEXPORT double	hypot(double, double);
OLM_DLLEXPORT int	ilogb(double) __pure2;
OLM_DLLEXPORT int	(isinf)(double) __pure2;
OLM_DLLEXPORT int	(isnan)(double) __pure2;
OLM_DLLEXPORT double	lgamma(double);
OLM_DLLEXPORT long long llrint(double);
OLM_DLLEXPORT long long llround(double);
OLM_DLLEXPORT double	log1p(double);
OLM_DLLEXPORT double	log2(double);
OLM_DLLEXPORT double	logb(double);
OLM_DLLEXPORT long	lrint(double);
OLM_DLLEXPORT long	lround(double);
OLM_DLLEXPORT double	nan(const char *) __pure2;
OLM_DLLEXPORT double	nextafter(double, double);
OLM_DLLEXPORT double	remainder(double, double);
OLM_DLLEXPORT double	remquo(double, double, int *);
OLM_DLLEXPORT double	rint(double);
#endif /* __BSD_VISIBLE || __ISO_C_VISIBLE >= 1999 || __XSI_VISIBLE */

#if __BSD_VISIBLE || __XSI_VISIBLE
OLM_DLLEXPORT double	j0(double);
OLM_DLLEXPORT double	j1(double);
OLM_DLLEXPORT double	jn(int, double);
OLM_DLLEXPORT double	y0(double);
OLM_DLLEXPORT double	y1(double);
OLM_DLLEXPORT double	yn(int, double);
#endif /* __BSD_VISIBLE || __XSI_VISIBLE */

#if __BSD_VISIBLE || __ISO_C_VISIBLE >= 1999
OLM_DLLEXPORT double	copysign(double, double) __pure2;
OLM_DLLEXPORT double	fdim(double, double);
OLM_DLLEXPORT double	fmax(double, double) __pure2;
OLM_DLLEXPORT double	fmin(double, double) __pure2;
OLM_DLLEXPORT double	nearbyint(double);
OLM_DLLEXPORT double	round(double);
OLM_DLLEXPORT double	scalbln(double, long);
OLM_DLLEXPORT double	scalbn(double, int);
OLM_DLLEXPORT double	tgamma(double);
OLM_DLLEXPORT double	trunc(double);
#endif

/*
 * BSD math library entry points
 */
#if __BSD_VISIBLE
OLM_DLLEXPORT int	isnanf(float) __pure2;

/*
 * Reentrant version of lgamma; passes signgam back by reference as the
 * second argument; user must allocate space for signgam.
 */
OLM_DLLEXPORT double	lgamma_r(double, int *);

/*
 * Single sine/cosine function.
 */
OLM_DLLEXPORT void	sincos(double, double *, double *);
#endif /* __BSD_VISIBLE */

/* float versions of ANSI/POSIX functions */
#if __ISO_C_VISIBLE >= 1999
OLM_DLLEXPORT float	acosf(float);
OLM_DLLEXPORT float	asinf(float);
OLM_DLLEXPORT float	atanf(float);
OLM_DLLEXPORT float	atan2f(float, float);
OLM_DLLEXPORT float	cosf(float);
OLM_DLLEXPORT float	sinf(float);
OLM_DLLEXPORT float	tanf(float);

OLM_DLLEXPORT float	coshf(float);
OLM_DLLEXPORT float	sinhf(float);
OLM_DLLEXPORT float	tanhf(float);

OLM_DLLEXPORT float	exp2f(float);
OLM_DLLEXPORT float	expf(float);
OLM_DLLEXPORT float	expm1f(float);
OLM_DLLEXPORT float	frexpf(float, int *);	/* fundamentally !__pure2 */
OLM_DLLEXPORT int	ilogbf(float) __pure2;
OLM_DLLEXPORT float	ldexpf(float, int);
OLM_DLLEXPORT float	log10f(float);
OLM_DLLEXPORT float	log1pf(float);
OLM_DLLEXPORT float	log2f(float);
OLM_DLLEXPORT float	logf(float);
OLM_DLLEXPORT float	modff(float, float *);	/* fundamentally !__pure2 */

OLM_DLLEXPORT float	powf(float, float);
OLM_DLLEXPORT float	sqrtf(float);

OLM_DLLEXPORT float	ceilf(float);
OLM_DLLEXPORT float	fabsf(float) __pure2;
OLM_DLLEXPORT float	floorf(float);
OLM_DLLEXPORT float	fmodf(float, float);
OLM_DLLEXPORT float	roundf(float);

OLM_DLLEXPORT float	erff(float);
OLM_DLLEXPORT float	erfcf(float);
OLM_DLLEXPORT float	hypotf(float, float);
OLM_DLLEXPORT float	lgammaf(float);
OLM_DLLEXPORT float	tgammaf(float);

OLM_DLLEXPORT float	acoshf(float);
OLM_DLLEXPORT float	asinhf(float);
OLM_DLLEXPORT float	atanhf(float);
OLM_DLLEXPORT float	cbrtf(float);
OLM_DLLEXPORT float	logbf(float);
OLM_DLLEXPORT float	copysignf(float, float) __pure2;
OLM_DLLEXPORT long long llrintf(float);
OLM_DLLEXPORT long long llroundf(float);
OLM_DLLEXPORT long	lrintf(float);
OLM_DLLEXPORT long	lroundf(float);
OLM_DLLEXPORT float	nanf(const char *) __pure2;
OLM_DLLEXPORT float	nearbyintf(float);
OLM_DLLEXPORT float	nextafterf(float, float);
OLM_DLLEXPORT float	remainderf(float, float);
OLM_DLLEXPORT float	remquof(float, float, int *);
OLM_DLLEXPORT float	rintf(float);
OLM_DLLEXPORT float	scalblnf(float, long);
OLM_DLLEXPORT float	scalbnf(float, int);
OLM_DLLEXPORT float	truncf(float);

OLM_DLLEXPORT float	fdimf(float, float);
OLM_DLLEXPORT float	fmaf(float, float, float);
OLM_DLLEXPORT float	fmaxf(float, float) __pure2;
OLM_DLLEXPORT float	fminf(float, float) __pure2;
#endif

/*
 * float versions of BSD math library entry points
 */
#if __BSD_VISIBLE
OLM_DLLEXPORT float	dremf(float, float);
OLM_DLLEXPORT float	j0f(float);
OLM_DLLEXPORT float	j1f(float);
OLM_DLLEXPORT float	jnf(int, float);
OLM_DLLEXPORT float	y0f(float);
OLM_DLLEXPORT float	y1f(float);
OLM_DLLEXPORT float	ynf(int, float);

/*
 * Float versions of reentrant version of lgamma; passes signgam back by
 * reference as the second argument; user must allocate space for signgam.
 */
OLM_DLLEXPORT float	lgammaf_r(float, int *);

/*
 * Single sine/cosine function.
 */
OLM_DLLEXPORT void	sincosf(float, float *, float *);
#endif	/* __BSD_VISIBLE */

/*
 * long double versions of ISO/POSIX math functions
 */
#if __ISO_C_VISIBLE >= 1999
OLM_DLLEXPORT long double	acoshl(long double);
OLM_DLLEXPORT long double	acosl(long double);
OLM_DLLEXPORT long double	asinhl(long double);
OLM_DLLEXPORT long double	asinl(long double);
OLM_DLLEXPORT long double	atan2l(long double, long double);
OLM_DLLEXPORT long double	atanhl(long double);
OLM_DLLEXPORT long double	atanl(long double);
OLM_DLLEXPORT long double	cbrtl(long double);
OLM_DLLEXPORT long double	ceill(long double);
OLM_DLLEXPORT long double	copysignl(long double, long double) __pure2;
OLM_DLLEXPORT long double	coshl(long double);
OLM_DLLEXPORT long double	cosl(long double);
OLM_DLLEXPORT long double	erfcl(long double);
OLM_DLLEXPORT long double	erfl(long double);
OLM_DLLEXPORT long double	exp2l(long double);
OLM_DLLEXPORT long double	expl(long double);
OLM_DLLEXPORT long double	expm1l(long double);
OLM_DLLEXPORT long double	fabsl(long double) __pure2;
OLM_DLLEXPORT long double	fdiml(long double, long double);
OLM_DLLEXPORT long double	floorl(long double);
OLM_DLLEXPORT long double	fmal(long double, long double, long double);
OLM_DLLEXPORT long double	fmaxl(long double, long double) __pure2;
OLM_DLLEXPORT long double	fminl(long double, long double) __pure2;
OLM_DLLEXPORT long double	fmodl(long double, long double);
OLM_DLLEXPORT long double	frexpl(long double value, int *); /* fundamentally !__pure2 */
OLM_DLLEXPORT long double	hypotl(long double, long double);
OLM_DLLEXPORT int		ilogbl(long double) __pure2;
OLM_DLLEXPORT long double	ldexpl(long double, int);
OLM_DLLEXPORT long double	lgammal(long double);
OLM_DLLEXPORT long long	llrintl(long double);
OLM_DLLEXPORT long long	llroundl(long double);
OLM_DLLEXPORT long double	log10l(long double);
OLM_DLLEXPORT long double	log1pl(long double);
OLM_DLLEXPORT long double	log2l(long double);
OLM_DLLEXPORT long double	logbl(long double);
OLM_DLLEXPORT long double	logl(long double);
OLM_DLLEXPORT long		lrintl(long double);
OLM_DLLEXPORT long		lroundl(long double);
OLM_DLLEXPORT long double	modfl(long double, long double *); /* fundamentally !__pure2 */
OLM_DLLEXPORT long double	nanl(const char *) __pure2;
OLM_DLLEXPORT long double	nearbyintl(long double);
OLM_DLLEXPORT long double	nextafterl(long double, long double);
OLM_DLLEXPORT double		nexttoward(double, long double);
OLM_DLLEXPORT float		nexttowardf(float, long double);
OLM_DLLEXPORT long double	nexttowardl(long double, long double);
OLM_DLLEXPORT long double	powl(long double, long double);
OLM_DLLEXPORT long double	remainderl(long double, long double);
OLM_DLLEXPORT long double	remquol(long double, long double, int *);
OLM_DLLEXPORT long double	rintl(long double);
OLM_DLLEXPORT long double	roundl(long double);
OLM_DLLEXPORT long double	scalblnl(long double, long);
OLM_DLLEXPORT long double	scalbnl(long double, int);
OLM_DLLEXPORT long double	sinhl(long double);
OLM_DLLEXPORT long double	sinl(long double);
OLM_DLLEXPORT long double	sqrtl(long double);
OLM_DLLEXPORT long double	tanhl(long double);
OLM_DLLEXPORT long double	tanl(long double);
OLM_DLLEXPORT long double	tgammal(long double);
OLM_DLLEXPORT long double	truncl(long double);
#endif /* __ISO_C_VISIBLE >= 1999 */

/* Reentrant version of lgammal. */
#if __BSD_VISIBLE
OLM_DLLEXPORT long double	lgammal_r(long double, int *);

/*
 * Single sine/cosine function.
 */
OLM_DLLEXPORT void	sincosl(long double, long double *, long double *);
#endif	/* __BSD_VISIBLE */

#if defined(__cplusplus)
}
#endif
#endif /* !OPENLIBM_MATH_H */

#endif /* OPENLIBM_USE_HOST_MATH_H */
