/*
 * ffsl(3). ffsll.c only defines it for the dyld simulator variant, where
 * macOS's libsystem_platform otherwise provides it.
 */
#include <strings.h>

int
ffsl(long mask)
{
	return __builtin_ffsl(mask);
}
