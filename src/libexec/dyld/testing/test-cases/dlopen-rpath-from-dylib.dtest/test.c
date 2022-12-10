
#include <stdio.h>
#include <dlfcn.h>
#include <stdbool.h>

#include "test_support.h"

void test_dlopen()
{
    void* handle = dlopen("@rpath/libbar.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlopen(\"@rpath/libbar.dylib\") failed: %s", dlerror());
    }
    dlclose(handle);
}

