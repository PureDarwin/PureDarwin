/* s_sincosf.c -- float version of s_sincos.c
 *
 * Copyright (C) 2013 Elliot Saba
 * Developed at the University of Washington
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
*/

#include "cdefs-compat.h"

#include <float.h>
#include <openlibm_math.h>

//#define	INLINE_KERNEL_COSDF
//#define	INLINE_KERNEL_SINDF
//#define INLINE_REM_PIO2F
#include "math_private.h"
//#include "e_rem_pio2f.c"
//#include "k_cosf.c"
//#include "k_sinf.c"


/* Constants used in shortcircuits in sincosf */
static const double
sc1pio2 = 1*M_PI_2,			/* 0x3FF921FB, 0x54442D18 */
sc2pio2 = 2*M_PI_2,			/* 0x400921FB, 0x54442D18 */
sc3pio2 = 3*M_PI_2,			/* 0x4012D97C, 0x7F3321D2 */
sc4pio2 = 4*M_PI_2,			/* 0x401921FB, 0x54442D18 */

/* Constants used in polynomial approximation of sin/cos */
one =  1.0,
S1 = -0x15555554cbac77.0p-55,	/* -0.166666666416265235595 */
S2 =  0x111110896efbb2.0p-59,	/*  0.0083333293858894631756 */
S3 = -0x1a00f9e2cae774.0p-65,	/* -0.000198393348360966317347 */
S4 =  0x16cd878c3b46a7.0p-71,	/*  0.0000027183114939898219064 */
C0  = -0x1ffffffd0c5e81.0p-54,	/* -0.499999997251031003120 */
C1  =  0x155553e1053a42.0p-57,	/*  0.0416666233237390631894 */
C2  = -0x16c087e80f1e27.0p-62,	/* -0.00138867637746099294692 */
C3  =  0x199342e0ee5069.0p-68;	/*  0.0000243904487962774090654 */

static void
__kernel_sincosdf( double x, float * s, float * c )
{
	double r, w, z, v;
	z = x*x;
	w = z*z;

	/* cos-specific computation; equivalent to calling
     __kernel_cos(x,y) and storing in k_c*/
	r = C2+z*C3;
	double k_c = ((one+z*C0) + w*C1) + (w*z)*r;

	/* sin-specific computation; equivalent to calling
    __kernel_sin(x,y,1) and storing in k_s*/
	r = S3+z*S4;
	v = z*x;
	double k_s = (x + v*(S1+z*S2)) + v*w*r;

	*c = k_c;
	*s = k_s;
}

OLM_DLLEXPORT void
sincosf(float x, float * s, float * c) {
	// Worst approximation of sin and cos NA
	*s = x;
	*c = x;

	double y;
	float k_c, k_s;
	int32_t n, hx, ix;

	GET_FLOAT_WORD(hx,x);
	ix = hx & 0x7fffffff;

	if(ix <= 0x3f490fda) {		/* |x| ~<= pi/4 */
	    if(ix<0x39800000) {		/* |x| < 2**-12 */
			/* Check if x is exactly zero */
			if(((int)x)==0) {
				*s = x;
				*c = 1.0f;
				return;
			}
		}
		__kernel_sincosdf(x, s, c);
		return;
	}
	/* |x| ~<= 5*pi/4 */
	if (ix<=0x407b53d1) {
		/* |x| ~<= 3pi/4 */
	    if(ix<=0x4016cbe3) {
			if(hx>0) {
				__kernel_sincosdf( sc1pio2 - x, c, s );
			}
			else {
				__kernel_sincosdf( sc1pio2 + x, c, &k_s );
				*s = -k_s;
		    }
		} else {

			if(hx>0) {
				__kernel_sincosdf( sc2pio2 - x, s, &k_c );
				*c = -k_c;
			} else  {
				__kernel_sincosdf( -sc2pio2 - x, s, &k_c );
				*c = -k_c;
			}
		}
		return;
	}

	/* |x| ~<= 9*pi/4 */
	if(ix<=0x40e231d5) {
		/* |x|  ~> 7*pi/4 */
	    if(ix<=0x40afeddf) {	
			if(hx>0) {
				__kernel_sincosdf( x - sc3pio2, c, &k_s );
				*s = -k_s;
			} else {
				__kernel_sincosdf( x + sc3pio2, &k_c, s );
				*c = -k_c;
		    } 
		}
		else {
	    	if( hx > 0 ) {
	    		__kernel_sincosdf( x - sc4pio2, s, c );
	    	} else {
	    		__kernel_sincosdf( x + sc4pio2, s, c );
	    	}
	    }
		return;
	}

	/* cos(Inf or NaN) is NaN */
	else if(ix>=0x7f800000) {
		*c = *s = x-x;
	} else {
		/* general argument reduction needed */
		n = __ieee754_rem_pio2f(x,&y);

		switch(n&3) {
			case 0:
				__kernel_sincosdf( y, s, c );
				break;
			case 1:
				__kernel_sincosdf( -y, c, s );
				break;
			case 2: 
				__kernel_sincosdf( -y, s, &k_c);
				*c = -k_c;
				break;
			default:
				__kernel_sincosdf( -y, &k_c, &k_s );
				*c = -k_c;
				*s = -k_s;
				break;
	    }
	}

}
