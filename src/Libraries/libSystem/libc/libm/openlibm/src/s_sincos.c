/* @(#)s_sincos.c 5.1 13/07/15 */
/*
 * ====================================================
 * Copyright (C) 2013 Elliot Saba. All rights reserved.
 *
 * Developed at the University of Washington.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
 #include "cdefs-compat.h"

/* sincos(x, s, c)
 * Several applications need sine and cosine of the same
 * angle x. This function computes both at the same time,
 * and stores the results in *sin and *cos.
 *
 * kernel function:
 *	__kernel_sin		... sine function on [-pi/4,pi/4]
 *	__kernel_cos		... cose function on [-pi/4,pi/4]
 *	__ieee754_rem_pio2	... argument reduction routine
 *
 * Method.
 *      Borrow liberally from s_sin.c and s_cos.c, merging
 *  efforts where applicable and returning their values in
 * appropriate variables, thereby slightly reducing the
 * amount of work relative to just calling sin/cos(x) 
 * separately
 *
 * Special cases:
 *      Let trig be any of sin, cos, or tan.
 *      sincos(+-INF, s, c)  is NaN, with signals;
 *      sincos(NaN, s, c)    is that NaN;
 */

#include <float.h>
#include <openlibm_math.h>

//#define INLINE_REM_PIO2
#include "math_private.h"
//#include "e_rem_pio2.c"

/* Constants used in polynomial approximation of sin/cos */
static const double
one =  1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
half =  5.00000000000000000000e-01, /* 0x3FE00000, 0x00000000 */
S1  = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
S2  =  8.33333333332248946124e-03, /* 0x3F811111, 0x1110F8A6 */
S3  = -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
S4  =  2.75573137070700676789e-06, /* 0x3EC71DE3, 0x57B1FE7D */
S5  = -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
S6  =  1.58969099521155010221e-10, /* 0x3DE5D93A, 0x5ACFD57C */
C1  =  4.16666666666666019037e-02, /* 0x3FA55555, 0x5555554C */
C2  = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
C3  =  2.48015872894767294178e-05, /* 0x3EFA01A0, 0x19CB1590 */
C4  = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
C5  =  2.08757232129817482790e-09, /* 0x3E21EE9E, 0xBDB4B1C4 */
C6  = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

static void
__kernel_sincos( double x, double y, int iy, double * k_s, double * k_c )
{
    /* Inline calculation of sin/cos, as we can save
    some work, and we will always need to calculate
    both values, no matter the result of switch */
    double z, w, r, v, hz;
    z   = x*x;
    w   = z*z;

    /* cos-specific computation; equivalent to calling
     __kernel_cos(x,y) and storing in k_c*/
    r   = z*(C1+z*(C2+z*C3)) + w*w*(C4+z*(C5+z*C6));
    hz  = 0.5*z;
    v   = one-hz;

    *k_c = v + (((one-v)-hz) + (z*r-x*y));

    /* sin-specific computation; equivalent to calling
    __kernel_sin(x,y,1) and storing in k_s*/
    r   = S2+z*(S3+z*S4) + z*w*(S5+z*S6);
    v   = z*x;
    if(iy == 0)
        *k_s = x+v*(S1+z*r);
    else
        *k_s = x-((z*(half*y-v*r)-y)-v*S1);
}

OLM_DLLEXPORT void
sincos(double x, double * s, double * c)
{
    double y[2];
    int32_t ix;

    /* Store high word of x in ix */
    GET_HIGH_WORD(ix,x);

    /* |x| ~< pi/4 */
    ix &= 0x7fffffff;
    if(ix <= 0x3fe921fb) {
        /* Check for small x for sin and cos */
        if(ix<0x3e46a09e) {
            /* Check for exact zero */
            if( (int)x==0 ) {
                *s = x;
                *c = 1.0;
                return;
            }
        }
        /* Call kernel function with 0 extra */
        __kernel_sincos(x,0.0,0, s, c);
    } else if( ix >= 0x7ff00000 ) {
        /* sincos(Inf or NaN) is NaN */
        *s = x-x;
        *c = x-x;
    }

    /*argument reduction needed*/
    else {
        double k_c, k_s;

        /* Calculate remainer, then sub out to kernel */
        int32_t n = __ieee754_rem_pio2(x,y);
        __kernel_sincos( y[0], y[1], 1, &k_s, &k_c );

        /* Figure out permutation of sin/cos outputs to true outputs */
        switch(n&3) {
            case 0:
                *c =  k_c;
                *s =  k_s;
                break;
            case 1:
                *c = -k_s;
                *s =  k_c;
                break;
            case 2:
                *c = -k_c;
                *s = -k_s;
                break;
            default:
                *c =  k_s;
                *s = -k_c;
                break;
        }
    }
}

#if (LDBL_MANT_DIG == 53)
__weak_reference(sincos, sincosl);
#endif
