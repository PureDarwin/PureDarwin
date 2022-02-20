
#include <stdio.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include "test_support.h"

void foo() {
	// foo can't see libFoundation.dylib as it's name isn't correct
    void* handle = dlopen("libFoundation.dylib", RTLD_LAZY);
    if ( handle != NULL ) {
        FAIL("dlopen-jna, libfoo should not be able to dlopen()");
    }

    // foo can't see libSecurity.dylib as it's name isn't correct
    void* handle2 = dlopen("libSecurity.dylib", RTLD_LAZY);
    if ( handle2 != NULL ) {
        FAIL("dlopen-jna, libfoo should not be able to dlopen()");
    }

    // foo can't see libCarbon.dylib as it's name isn't correct
    void* handle3 = dlopen("libCarbon.dylib", RTLD_LAZY);
    if ( handle3 != NULL ) {
        FAIL("dlopen-jna, libfoo should not be able to dlopen()");
    }
}
