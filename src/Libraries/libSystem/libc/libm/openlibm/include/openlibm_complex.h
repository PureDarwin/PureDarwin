/*	$OpenBSD: complex.h,v 1.5 2014/03/16 18:38:30 guenther Exp $	*/
/*
 * Copyright (c) 2008 Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef OPENLIBM_USE_HOST_COMPLEX_H
#include <complex.h>
#else /* !OPENLIBM_USE_HOST_COMPLEX_H */

#ifndef OPENLIBM_COMPLEX_H
#define	OPENLIBM_COMPLEX_H

#define complex _Complex

#define _Complex_I 1.0fi
#define I _Complex_I

/*
 * Macros that can be used to construct complex values.
 *
 * The C99 standard intends x+I*y to be used for this, but x+I*y is
 * currently unusable in general since gcc introduces many overflow,
 * underflow, sign and efficiency bugs by rewriting I*y as
 * (0.0+I)*(y+0.0*I) and laboriously computing the full complex product.
 * In particular, I*Inf is corrupted to NaN+I*Inf, and I*-0 is corrupted
 * to -0.0+I*0.0.
 *
 * In C11, a CMPLX(x,y) macro was added to circumvent this limitation,
 * and gcc 4.7 added a __builtin_complex feature to simplify implementation
 * of CMPLX in libc, so we can take advantage of these features if they
 * are available. Clang simply allows complex values to be constructed
 * using a compound literal.
 *
 * If __builtin_complex is not available, resort to using inline
 * functions instead. These can unfortunately not be used to construct
 * compile-time constants.
 *
 * C99 specifies that complex numbers have the same representation as
 * an array of two elements, where the first element is the real part
 * and the second element is the imaginary part.
 */

#ifdef __clang__
#  define CMPLXF(x, y) ((float complex){x, y})
#  define CMPLX(x, y) ((double complex){x, y})
#  define CMPLXL(x, y) ((long double complex){x, y})
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__INTEL_COMPILER)
#  define CMPLXF(x,y) __builtin_complex ((float) (x), (float) (y))
#  define CMPLX(x,y) __builtin_complex ((double) (x), (double) (y))
#  define CMPLXL(x,y) __builtin_complex ((long double) (x), (long double) (y))
#else
static inline float complex
CMPLXF(float x, float y)
{
	union {
		float a[2];
		float complex f;
	} z = {{ x, y }};

	return (z.f);
}

static inline double complex
CMPLX(double x, double y)
{
	union {
		double a[2];
		double complex f;
	} z = {{ x, y }};

	return (z.f);
}

static inline long double complex
CMPLXL(long double x, long double y)
{
	union {
		long double a[2];
		long double complex f;
	} z = {{ x, y }};

	return (z.f);
}
#endif

/*
 * Double versions of C99 functions
 */
double complex cacos(double complex);
double complex casin(double complex);
double complex catan(double complex);
double complex ccos(double complex);
double complex csin(double complex);
double complex ctan(double complex);
double complex cacosh(double complex);
double complex casinh(double complex);
double complex catanh(double complex);
double complex ccosh(double complex);
double complex csinh(double complex);
double complex ctanh(double complex);
double complex cexp(double complex);
double complex clog(double complex);
double cabs(double complex);
double complex cpow(double complex, double complex);
double complex csqrt(double complex);
double carg(double complex);
double cimag(double complex);
double complex conj(double complex);
double complex cproj(double complex);
double creal(double complex);

/*
 * Float versions of C99 functions
 */
float complex cacosf(float complex);
float complex casinf(float complex);
float complex catanf(float complex);
float complex ccosf(float complex);
float complex csinf(float complex);
float complex ctanf(float complex);
float complex cacoshf(float complex);
float complex casinhf(float complex);
float complex catanhf(float complex);
float complex ccoshf(float complex);
float complex csinhf(float complex);
float complex ctanhf(float complex);
float complex cexpf(float complex);
float complex clogf(float complex);
float cabsf(float complex);
float complex cpowf(float complex, float complex);
float complex csqrtf(float complex);
float cargf(float complex);
float cimagf(float complex);
float complex conjf(float complex);
float complex cprojf(float complex);
float crealf(float complex);

/*
 * Long double versions of C99 functions
 */
long double complex cacosl(long double complex);
long double complex casinl(long double complex);
long double complex catanl(long double complex);
long double complex ccosl(long double complex);
long double complex csinl(long double complex);
long double complex ctanl(long double complex);
long double complex cacoshl(long double complex);
long double complex casinhl(long double complex);
long double complex catanhl(long double complex);
long double complex ccoshl(long double complex);
long double complex csinhl(long double complex);
long double complex ctanhl(long double complex);
long double complex cexpl(long double complex);
long double complex clogl(long double complex);
long double cabsl(long double complex);
long double complex cpowl(long double complex,
	long double complex);
long double complex csqrtl(long double complex);
long double cargl(long double complex);
long double cimagl(long double complex);
long double complex conjl(long double complex);
long double complex cprojl(long double complex);
long double creall(long double complex);

#endif /* !OPENLIBM_COMPLEX_H */

#endif /* OPENLIBM_USE_HOST_COMPLEX_H */
