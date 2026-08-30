/*
 * Kernel.framework compatibility header.
 * The implementation-facing qsort declaration is also exported by bsd/kern,
 * but scheduler headers include it through the kern header namespace.
 */
#ifndef _KERN_QSORT_H_
#define _KERN_QSORT_H_

#include <stddef.h>

__BEGIN_DECLS

typedef int (*cmpfunc_t)(const void *a, const void *b);

__private_extern__
void qsort(void *array, size_t num_elements, size_t element_size,
    cmpfunc_t compare);

__END_DECLS

#endif
