#ifndef AIRJITD_FIBER_H
#define AIRJITD_FIBER_H

#include <stddef.h>

// saves the running stack's pointer in *save and continues on to
void pd_fiber_switch(void **save, void *to);

// a stack that, switched to, calls fn(arg), fn must never return (it switches away for good)
void *pd_fiber_init(void *stack, size_t size, void (*fn)(void *), void *arg);

#endif
