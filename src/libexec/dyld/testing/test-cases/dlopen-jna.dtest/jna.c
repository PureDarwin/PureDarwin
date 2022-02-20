
#include <stdio.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include "test_support.h"

void jna() {
    // jna should see libFoundation.dylib as it's name is correct
    void* handle = dlopen("libFoundation.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlopen-jna, libjna not be able to dlopen(): %s", dlerror());
    }

    // jna should see libSecurity.dylib as it's name is correct
    void* handle2 = dlopen("libSecurity.dylib", RTLD_LAZY);
    if ( handle2 == NULL ) {
        FAIL("dlopen-jna, libjna not be able to dlopen(): %s", dlerror());
    }

    // jna should see libCarbon.dylib as it's name is correct
    void* handle3 = dlopen("libCarbon.dylib", RTLD_LAZY);
    if ( handle3 == NULL ) {
        FAIL("dlopen-jna, libjna not be able to dlopen(): %s", dlerror());
    }
}
