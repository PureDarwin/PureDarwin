#ifndef _QSORT_R_H
#define _QSORT_R_H
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif
void qsort_r_local(void *base, size_t nmemb, size_t size, void *thunk, int (*compar)(void *, const void *, const void *));
#undef qsort_r
#define qsort_r qsort_r_local
#if defined(__cplusplus)
};
#endif

#endif
