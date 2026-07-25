/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 *
 * Modified by Steven G. Kargl for the long double type.
 */

#include "cdefs-compat.h"
//__FBSDID("$FreeBSD: src/lib/msun/src/w_cabsl.c,v 1.1 2008/03/30 20:02:03 das Exp $");

#include <openlibm_complex.h>
#include <openlibm_math.h>

#include "math_private.h"

OLM_DLLEXPORT long double
cabsl(long double complex z)
{
	return hypotl(creall(z), cimagl(z));
}
