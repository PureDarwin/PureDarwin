
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/dyld-interposing.h>

#include "test_support.h"

static bool inMalloc = false;
static bool forceSystemMalloc = false;

void* mymalloc(size_t size)
{
    // We are in our own printf, so we need to fall back to the default malloc
    if (forceSystemMalloc) {
        return malloc(size);
    }

    if (inMalloc) {
        // Recursion!   This shouldn't happen.
        forceSystemMalloc = true;
        FAIL("mymalloc() is recursive");
    }

    inMalloc = true;

    // ASan calls dlsym before libdyld has created an image list.  Make sure that succeeds
    void* sym = dlsym(RTLD_DEFAULT, "malloc");
    if (sym == NULL) {
        forceSystemMalloc = true;
        FAIL("dlsym failed");
    }

    if (sym != mymalloc) {
        forceSystemMalloc = true;
        FAIL("dlsym result %p != mymalloc %p", sym, &mymalloc);
    }
    void* result = malloc(size);

    inMalloc = false;

    return result;
}

DYLD_INTERPOSE(mymalloc, malloc)
