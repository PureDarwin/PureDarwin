#pragma once
// linux stand-in: no darwin sysctls, callers fall back to their defaults
#include <stddef.h>
static inline int sysctlbyname(const char *n, void *o, size_t *l, void *nw, size_t nl) { (void)n; (void)o; (void)l; (void)nw; (void)nl; return -1; }
