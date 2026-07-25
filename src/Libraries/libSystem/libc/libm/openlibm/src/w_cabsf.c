/*
 * cabsf() wrapper for hypotf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <openlibm_complex.h>
#include <openlibm_math.h>

#include "math_private.h"

OLM_DLLEXPORT float
cabsf(z)
	float complex z;
{

	return hypotf(crealf(z), cimagf(z));
}
