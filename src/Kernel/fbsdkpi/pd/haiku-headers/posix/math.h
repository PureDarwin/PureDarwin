/*
 * Copyright 2001-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MATH_H_
#define _MATH_H_

/* (float|double)_t typedefs */

#ifdef __FLT_EVAL_METHOD__
	#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#else
	#define FLT_EVAL_METHOD 0
#endif

#if FLT_EVAL_METHOD == 1
	typedef double float_t;
	typedef double double_t;
#elif FLT_EVAL_METHOD == 2
	typedef long double float_t;
	typedef long double double_t;
#else
	typedef float float_t;
	typedef double double_t;
#endif

#define M_E				2.7182818284590452354	/* e */
#define M_LOG2E			1.4426950408889634074	/* log 2e */
#define M_LOG10E		0.43429448190325182765	/* log 10e */
#define M_LN2			0.69314718055994530942	/* log e2 */
#define M_LN10			2.30258509299404568402	/* log e10 */
#define M_PI			3.14159265358979323846	/* pi */
#define M_PI_2			1.57079632679489661923	/* pi/2 */
#define M_PI_4			0.78539816339744830962	/* pi/4 */
#define M_1_PI			0.31830988618379067154	/* 1/pi */
#define M_2_PI			0.63661977236758134308	/* 2/pi */
#define M_2_SQRTPI		1.12837916709551257390	/* 2/sqrt(pi) */
#define M_SQRT2			1.41421356237309504880	/* sqrt(2) */
#define M_SQRT1_2		0.70710678118654752440	/* 1/sqrt(2) */

/* platform independent IEEE floating point special values */
#if __GNUC__ >= 4
	/* gcc4 and clang definitions */
#	ifndef HUGE_VAL
#		define HUGE_VAL     __builtin_huge_val()
#	endif

#	define HUGE_VALF        __builtin_huge_valf()
#	define HUGE_VALL		__builtin_huge_vall()
#	define NAN              __builtin_nanf("")
#	define INFINITY			__builtin_inff()
#else
	/* gcc2 definitions */
#	define	__HUGE_VAL_v	0x7ff0000000000000LL
#	define __huge_val_t	union { unsigned char __c[8]; long long __ll; double __d; }
#	ifndef HUGE_VAL
#		define HUGE_VAL		(((__huge_val_t) { __ll: __HUGE_VAL_v }).__d)
#	endif

#	define __HUGE_VALF_v	0x7f800000L
#	define __huge_valf_t	union { unsigned char __c[4]; long __l; float __f; }
#	define HUGE_VALF		(((__huge_valf_t) { __l: __HUGE_VALF_v }).__f)

	/* TODO: define HUGE_VALL for long doubles */

#	define __NAN_VALF_v	0x7fc00000L
#	define NAN				(((__huge_valf_t) { __l: __NAN_VALF_v }).__f)
#	define INFINITY			HUGE_VALF
#endif

#include <limits.h>
#define MAXFLOAT FLT_MAX

/* floating-point categories */
#define FP_NAN			0
#define FP_INFINITE		1
#define FP_ZERO			2
#define FP_SUBNORMAL	3
#define FP_NORMAL		4

#ifdef __FP_FAST_FMA
	#define FP_FAST_FMA		__FP_FAST_FMA
#else
	#define FP_FAST_FMA		0
#endif
#ifdef __FP_FAST_FMAF
	#define FP_FAST_FMAF	__FP_FAST_FMAF
#else
	#define FP_FAST_FMAF	0
#endif
#ifdef __FP_FAST_FMAL
	#define FP_FAST_FMAL 	__FP_FAST_FMAL
#else
	#define FP_FAST_FMAL	0
#endif

#define FP_ILOGB0		INT_MIN
#define FP_ILOGBNAN		INT_MAX

#ifdef __cplusplus
struct __exception;
extern "C" int matherr(struct __exception *);
struct __exception {
#else
struct exception;
extern int matherr(struct exception *);
struct exception {
#endif
	int		type;
	char	*name;
	double	arg1;
	double	arg2;
	double	retval;
};

#define MATH_ERRNO			1
#define MATH_ERREXCEPT		2
#define math_errhandling	0

#define DOMAIN		1
#define SING		2
#define OVERFLOW	3
#define UNDERFLOW	4
#define TLOSS		5
#define PLOSS		6

extern int signgam;


#ifdef __cplusplus
extern "C" {
#endif

/* float math functions */
extern float		acosf(float x);
extern float		acoshf(float x);
extern float		asinf(float x);
extern float		asinhf(float x);
extern float		atan2f(float y, float x);
extern float		atanf(float x);
extern float		atanhf(float x);
extern float		cbrtf(float x);
extern float		ceilf(float x);
extern float		copysignf(float x, float y);
extern float		cosf(float x);
extern float		coshf(float x);
extern float		erfcf(float x);
extern float		erff(float x);
extern float		expf(float x);
extern float		exp2f(float x);
extern float		expm1f(float x);
extern float		fabsf(float x);
extern float		fdimf(float x, float y);
extern float		floorf(float x);
extern float		fmaf(float x, float y, float z);
extern float		fmaxf(float x, float y);
extern float		fminf(float x, float y);
extern float		fmodf(float x, float y);
extern float		frexpf(float x, int *_exponent);
extern float		gammaf(float x);
extern float		hypotf(float x, float y);
extern int			ilogbf(float x);
extern float		ldexpf(float x, int exponent);
extern float		lgammaf(float x);
extern long long	llrintf(float x);
extern long long	llroundf(float x);
extern float		log10f(float x);
extern float		log1pf(float x);
extern float		log2f(float x);
extern float		logbf(float x);
extern float		logf(float x);
extern long			lrintf(float x);
extern long			lroundf(float x);
extern float		modff(float x, float *y);
extern float		nanf(const char *tagp);
extern float		nearbyintf(float x);
extern float		nextafterf(float x, float y);
extern float		nexttowardf(float x, long double y);
extern float		powf(float x, float y);
extern float		remainderf(float x, float y);
extern float		remquof(float x, float y, int *quo);
extern float		rintf(float x);
extern float		roundf(float x);
extern float		scalbf(float x, float n);
extern float		scalblnf(float x, long n);
extern float		scalbnf(float x, int n);
extern float		sinf(float x);
extern float		sinhf(float x);
extern float		sqrtf(float x);
extern float		tanf(float x);
extern float		tanhf(float x);
extern float		tgammaf(float x);
extern float		truncf(float x);

/* double math functions */
extern double		acos(double x);
extern double		acosh(double x);
extern double		asin(double x);
extern double		asinh(double x);
extern double		atan(double x);
extern double		atan2(double x, double y);
extern double		atanh(double x);
extern double		cbrt(double x);
extern double		ceil(double x);
extern double		copysign(double x, double y);
extern double		cos(double x);
extern double		cosh(double x);
extern double		erf(double x);
extern double		erfc(double x);
extern double		exp(double x);
extern double		expm1(double x);
extern double		exp2(double x);
extern double		fabs(double x);
extern double		fdim(double x, double y);
extern double		floor(double x);
extern double		fma(double x, double y, double z);
extern double		fmax(double x, double y);
extern double		fmin(double x, double y);
extern double		fmod(double x, double y);
extern double		frexp(double x, int *_exponent);
extern double		gamma(double x);
extern double		hypot(double x, double y);
extern int			ilogb(double x);
extern double		ldexp(double x, int exponent);
extern double		lgamma(double x);
extern long long	llrint(double x);
extern long long	llround(double x);
extern double		log(double x);
extern double		log10(double x);
extern double		log1p(double x);
extern double		log2(double x);
extern double		logb(double x);
extern long			lrint(double x);
extern long			lround(double x);
extern double		modf(double x, double *y);
extern double		nan(const char* tagp);
extern double		nearbyint(double x);
extern double		nextafter(double x, double y);
extern double		nexttoward(double x, long double y);
extern double		pow(double x, double y);
extern double		remainder(double x, double y);
extern double		remquo(double x, double y, int *quo);
extern double		rint(double x);
extern double		round(double x);
extern double		scalb(double x, double y);
extern double 		scalbln(double x, long n);
extern double		scalbn(double x, int y);
extern double		sin(double x);
extern double		sinh(double x);
extern double		sqrt(double x);
extern double		tan(double x);
extern double		tanh(double x);
extern double		tgamma(double x);
extern double		trunc(double x);

/* long double math functions */
extern long double	acoshl(long double x);
extern long double	acosl(long double x);
extern long double	asinhl(long double x);
extern long double	asinl(long double x);
extern long double	atanhl(long double x);
extern long double	atanl(long double x);
extern long double	atan2l(long double y, long double x);
extern long double	cbrtl(long double x);
extern long double	ceill(long double x);
extern long double	copysignl(long double x, long double y);
extern long double	cosl(long double x);
extern long double	coshl(long double x);
extern long double	erfcl(long double x);
extern long double	erfl(long double x);
extern long double	expl(long double x);
extern long double	expm1l(long double x);
extern long double	exp2l(long double x);
extern long double	fabsl(long double x);
extern long double	fdiml(long double x, long double y);
extern long double	floorl(long double x);
extern long double	fmal(long double x, long double y, long double z);
extern long double	fmaxl(long double x, long double y);
extern long double	fminl(long double x, long double y);
extern long double	fmodl(long double x, long double y);
extern long double	frexpl(long double num, int *_exponent);
extern long double	hypotl(long double x, long double y);
extern int			ilogbl(long double x);
extern long double	ldexpl(long double x, int exponent);
extern long double	lgammal(long double x);
extern long long	llrintl(long double x);
extern long long	llroundl(long double x);
extern long double	logbl(long double x);
extern long double	logl(long double x);
extern long double	log1pl(long double x);
extern long double	log10l(long double x);
extern long double	log2l(long double x);
extern long			lrintl(long double x);
extern long			lroundl(long double x);
extern long double	modfl(long double value, long double *iptr);
extern long double	nanl(const char* tagp);
extern long double	nearbyintl(long double x);
extern long double	nextafterl(long double x, long double y);
extern long double	nexttowardl(long double x, long double y);
extern long double	powl(long double x, long double y);
extern long double	remainderl(long double x, long double y);
extern long double	remquol(long double x, long double y, int *quo);
extern long double	rintl(long double x);
extern long double	roundl(long double x);
extern long double	scalbnl(long double x, int n);
extern long double	scalblnl(long double x, long n);
extern long double	sinl(long double x);
extern long double	sinhl(long double x);
extern long double	sqrtl(long double x);
extern long double	tanl(long double x);
extern long double	tanhl(long double x);
extern long double	tgammal(long double x);
extern long double	truncl(long double x);

/* some BSD non-ANSI or POSIX math functions */
extern double		j0(double x);
extern double		j1(double x);
extern double		jn(int x, double y);
extern double		lgamma_r(double x, int *y);
extern double		y0(double x);
extern double		y1(double x);
extern double		yn(int x, double y);

/* other stuff as defined in BeOS */
extern int			isinff(float value);
extern int			finitef(float value);
extern float		dremf(float x, float y);
extern float		significandf(float x);
extern int			isnanf(float value);
extern double		significand(double x);
extern double		drem(double x, double y);
extern int			finite(double x);
extern float		j0f(float x);
extern float		j1f(float x);
extern float		jnf(int x, float y);
extern float		y0f(float x);
extern float		y1f(float x);
extern float		ynf(int x, float y);
extern float		lgammaf_r(float x, int *y);

#if __GNUC__ >= 7 || defined(__clang__)
#define fpclassify(value) \
	__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (value))
#define signbit(value)	__builtin_signbit((value))
#define isfinite(value)	__builtin_isfinite((value))
#define isnormal(value)	__builtin_isnormal((value))
#define isnan(value)	__builtin_isnan((value))
#define isinf(value)	__builtin_isinf_sign((value))
#else
/* prototypes for functions used in the macros below */
extern int			__fpclassifyf(float value);
extern int			__signbitf(float value);
extern int			__finitef(float value);
extern int			__isnanf(float value);
extern int			__isinff(float value);

extern int			__fpclassifyl(long double value);
extern int			__signbitl(long double value);
extern int			__finitel(long double value);
extern int			__isnanl(long double value);
extern int			__isinfl(long double value);

extern int			__fpclassify(double value);
extern int			__signbit(double value);
extern int			__finite(double value);
extern int			__isnan(double value);
extern int			__isinf(double value);

/* returns number of classification appropriate for 'value' */
#define fpclassify(value) \
	(sizeof(value) == sizeof(float) ? __fpclassifyf(value)		\
		: sizeof(value) == sizeof(double) ? __fpclassify(value)	\
		: __fpclassifyl(value))

/* returns non-zero if 'value' is negative */
# define signbit(value) \
	(sizeof(value) == sizeof(float) ? __signbitf(value)			\
		: sizeof(value) == sizeof(double) ? __signbit(value)	\
		: __signbitl(value))

/* returns non-zero if 'value' is not Inf or NaN */
# define isfinite(value) \
	(sizeof(value) == sizeof(float) ? __finitef(value)			\
		: sizeof(value) == sizeof(double) ? __finite(value)		\
		: __finitel(value))

/* returns non-zero if 'value' is neither zero, sub-normal, Inf, nor NaN */
# define isnormal(value) \
	(fpclassify(value) == FP_NORMAL)

/* returns non-zero if 'value' is NaN */
# define isnan(value) \
	(sizeof(value) == sizeof(float) ? __isnanf(value)			\
		: sizeof(value) == sizeof(double) ? __isnan(value)		\
		: __isnanl(value))

/* returns non-zero if 'value is Inf */
# define isinf(value) \
	(sizeof(value) == sizeof(float) ? __isinff(value)			\
		: sizeof(value) == sizeof(double) ? __isinf(value)		\
		: __isinfl(value))
#endif

#if __GNUC__ >= 4
	#define isgreater(x, y)			__builtin_isgreater((x), (y))
	#define isgreaterequal(x, y)	__builtin_isgreaterequal((x), (y))
	#define isless(x, y)			__builtin_isless((x), (y))
	#define islessequal(x, y)		__builtin_islessequal((x), (y))
	#define islessgreater(x, y)		__builtin_islessgreater((x), (y))
	#define isunordered(x, y)		__builtin_isunordered((x), (y))
#else
	/* these are only for GCC2, and require GNUC statement expressions */
	#define _wrap_expr_typeof(x, y, body) ({	\
		__typeof(x) X = (x);					\
		__typeof(y) Y = (y);					\
		body;})
	#define isgreater(x, y)	\
		_wrap_expr_typeof(x, y, !isnan(X) && !isnan(Y) && X > Y)
	#define isgreaterequal(x, y) \
		_wrap_expr_typeof(x, y, !isnan(X) && !isnan(Y) && X >= Y)
	#define isless(x, y) \
		_wrap_expr_typeof(x, y, !isnan(X) && !isnan(Y) && X < Y)
	#define islessequal(x, y) \
		_wrap_expr_typeof(x, y, !isnan(X) && !isnan(Y) && X <= Y)
	#define islessgreater(x, y)	\
		_wrap_expr_typeof(x, y, X < Y || Y < X)
	#define isunordered(x, y) \
		_wrap_expr_typeof(x, y, isnan(X) || isnan(Y))
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _MATH_H_ */
