/*
 * Copyright 2015, 2025 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _COMPLEX_H_
#define _COMPLEX_H_

#ifdef __GNUC__
#  if __STDC_VERSION__ < 199901L
#    define _Complex __complex__
#endif
#define _Complex_I ((float _Complex)1.0i)
#endif

#define complex _Complex
#define I _Complex_I

#ifdef _Imaginary_I
#define CMPLX(x, y) ((double complex)((double)(x) + _Imaginary_I * (double)(y)))
#define CMPLXF(x, y) ((float complex)((float)(x) + _Imaginary_I * (float)(y)))
#define CMPLXL(x, y) ((long double complex)((long double)(x) + _Imaginary_I * (long double)(y)))
#else
#define CMPLX(x, y) (__builtin_complex((double)(x), (double)(y)))
#define CMPLXF(x, y) (__builtin_complex((float)(x), (float)(y)))
#define CMPLXL(x, y) (__builtin_complex((long double)(x), (long double)(y)))
#endif

#ifdef  __cplusplus
extern "C" {
#endif

extern double               cabs(double complex);
extern float                cabsf(float complex);
extern long double          cabsl(long double complex);
extern double complex       cacos(double complex);
extern float complex        cacosf(float complex);
extern double complex       cacosh(double complex);
extern float complex        cacoshf(float complex);
extern long double complex  cacoshl(long double complex);
extern long double complex  cacosl(long double complex);
extern double               carg(double complex);
extern float                cargf(float complex);
extern long double          cargl(long double complex);
extern double complex       casin(double complex);
extern float complex        casinf(float complex);
extern double complex       casinh(double complex);
extern float complex        casinhf(float complex);
extern long double complex  casinhl(long double complex);
extern long double complex  casinl(long double complex);
extern double complex       catan(double complex);
extern float complex        catanf(float complex);
extern double complex       catanh(double complex);
extern float complex        catanhf(float complex);
extern long double complex  catanhl(long double complex);
extern long double complex  catanl(long double complex);
extern double complex       ccos(double complex);
extern float complex        ccosf(float complex);
extern double complex       ccosh(double complex);
extern float complex        ccoshf(float complex);
extern long double complex  ccoshl(long double complex);
extern long double complex  ccosl(long double complex);
extern double complex       cexp(double complex);
extern float complex        cexpf(float complex);
extern long double complex  cexpl(long double complex);
extern double               cimag(double complex);
extern float                cimagf(float complex);
extern long double          cimagl(long double complex);
extern double complex       clog(double complex);
extern float complex        clogf(float complex);
extern long double complex  clogl(long double complex);
extern double complex       conj(double complex);
extern float complex        conjf(float complex);
extern long double complex  conjl(long double complex);
extern double complex       cpow(double complex, double complex);
extern float complex        cpowf(float complex, float complex);
extern long double complex  cpowl(long double complex, long double complex);
extern double complex       cproj(double complex);
extern float complex        cprojf(float complex);
extern long double complex  cprojl(long double complex);
extern double               creal(double complex);
extern float                crealf(float complex);
extern long double          creall(long double complex);
extern double complex       csin(double complex);
extern float complex        csinf(float complex);
extern double complex       csinh(double complex);
extern float complex        csinhf(float complex);
extern long double complex  csinhl(long double complex);
extern long double complex  csinl(long double complex);
extern double complex       csqrt(double complex);
extern float complex        csqrtf(float complex);
extern long double complex  csqrtl(long double complex);
extern double complex       ctan(double complex);
extern float complex        ctanf(float complex);
extern double complex       ctanh(double complex);
extern float complex        ctanhf(float complex);
extern long double complex  ctanhl(long double complex);
extern long double complex  ctanl(long double complex);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif	/* _COMPLEX_H_ */
