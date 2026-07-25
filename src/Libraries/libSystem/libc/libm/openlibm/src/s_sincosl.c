/* s_sincosl.c -- long double version of s_sincos.c
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

#include "math_private.h"
#if LDBL_MANT_DIG == 64
#include "../ld80/e_rem_pio2l.h"
#elif LDBL_MANT_DIG == 113
#include "../ld128/e_rem_pio2l.h"
#else
#error "Unsupported long double format"
#endif

OLM_DLLEXPORT void
sincosl( long double x, long double * s, long double * c )
{
    *s = sinl( x );
    *c = cosl( x );
}
