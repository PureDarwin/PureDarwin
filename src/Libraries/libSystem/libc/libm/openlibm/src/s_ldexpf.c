#include "cdefs-compat.h"

#include <openlibm_math.h>

OLM_DLLEXPORT float
ldexpf(float x, int n)
{
	return scalbnf(x, n);
}
