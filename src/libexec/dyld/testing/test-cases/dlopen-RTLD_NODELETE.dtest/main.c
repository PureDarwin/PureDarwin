
// BUILD:  $CC foo.c -dynamiclib  -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC bar.c -dynamiclib  -install_name $RUN_DIR/libbar.dylib -o $BUILD_DIR/libbar.dylib
// BUILD:  $CC main.c -o $BUILD_DIR/dlopen-RTLD_NODELETE.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./dlopen-RTLD_NODELETE.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    ///
    /// This tests that RTLD_NODELETE on first dlopen() blocks dlclose() from unloading image
    ///
    void* handle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_NODELETE);
    if ( handle == NULL ) {
        FAIL("dlopen(libfoo.dylib, RTLD_NODELETE) failed but it should have worked: %s", dlerror());
    }
    int* fooSym = (int*)dlsym(handle, "foo");
    if ( fooSym == NULL ) {
        FAIL("dlsym(handle, \"foo\") failed but it should have worked: %s", dlerror());
    }
    int fooValue = *fooSym;
    dlclose(handle);
    Dl_info info;
    if ( dladdr(fooSym, &info) != 0 ) {
        FAIL("dladdr(fooSym, xx) succeeded as if libfoo.dylib was not unloaded");
    }
    // dereference foo pointer.  If RTLD_NODELETE worked, this will not crash
    if ( *fooSym != fooValue ) {
        FAIL("value at fooSym changed");
    }

    ///
    /// This tests that RTLD_NODELETE on later dlopen() blocks dlclose() from unloading image
    ///
    void* handle2 = dlopen(RUN_DIR "/libbar.dylib", RTLD_GLOBAL);
    if ( handle2 == NULL ) {
        FAIL("dlopen(libfoo.dylib, RTLD_GLOBAL) failed but it should have worked: %s", dlerror());
    }
    int* barSym = (int*)dlsym(handle2, "bar");
    if ( barSym == NULL ) {
        FAIL("dlsym(handle, \"bar\") failed but it should have worked: %s", dlerror());
    }
    int barValue = *barSym;
    void* handle3 = dlopen(RUN_DIR "/libbar.dylib", RTLD_NODELETE);
    if ( handle3 == NULL ) {
        FAIL("dlopen(libfoo.dylib, RTLD_NODELETE) failed but it should have worked: %s", dlerror());
    }
    dlclose(handle2);
    dlclose(handle3);
    if ( dladdr(barSym, &info) != 0 ) {
        FAIL("dladdr(barSym, xx) succeeded as if libbar.dylib was not unloaded");
    }
    // dereference foo pointer.  If RTLD_NODELETE worked, this will not crash
    if ( *barSym != barValue ) {
        FAIL("value at barSym changed");
    }

    PASS("Success");
}
