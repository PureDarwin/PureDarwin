#include "cdefs-compat.h"

#include <openlibm_math.h>

#include "math_private.h"

OLM_DLLEXPORT long double
lgammal(long double x)
{
#ifdef OPENLIBM_ONLY_THREAD_SAFE
	int signgam;
#endif

	return (lgammal_r(x, &signgam));
}
