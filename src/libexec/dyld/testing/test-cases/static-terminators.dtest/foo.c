#include <stddef.h>
#include <stdio.h>

#include "test_support.h"

extern void libDynamicTerminated();


static __attribute__((destructor))
void myTerm()
{
    LOG("foo static terminator");
    libDynamicTerminated();
}

