/*
 * bcmp(3): the legacy BSD spelling of memcmp(3), still referenced by older
 * BSD-derived sources (ping(8) from network_cmds among them). Apple's Libc
 * carries it as a plain alias; libc has no assembly variant here, so forward.
 */

#include <string.h>

int
bcmp(const void *s1, const void *s2, size_t n)
{
	return memcmp(s1, s2, n);
}
