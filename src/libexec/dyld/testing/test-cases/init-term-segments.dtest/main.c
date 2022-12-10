

// BUILD:  $CC foo.c -dynamiclib -fno-register-global-dtors-with-atexit -Wl,-segprot,__SOMETEXT,rx,rx -Wl,-segprot,__MORETEXT,rx,rx -o $BUILD_DIR/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib
// BUILD:  $CC main.c -o $BUILD_DIR/init-term-segments.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./init-term-segments.exe

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "test_support.h"

extern bool foo(bool* ptr);

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    void* h = dlopen(RUN_DIR "/libfoo.dylib", RTLD_NOW);
    if (h == NULL) {
		FAIL("dlerror = %s", dlerror());
    }

    void* fooSym = dlsym(RTLD_DEFAULT, "foo");
    if ( fooSym == NULL ) {
        FAIL("dlsym failure");
    }

    bool ranTerm = false;
    bool ranInit = ((__typeof(&foo))fooSym)(&ranTerm);
    if (!ranInit) {
        FAIL("didn't run init");
    }

    if ( dlclose(h) != 0 ) {
        FAIL("didn't dlclose");
    }

#if __arm64e__
    if (ranTerm) {
        FAIL("unexpectedly ran term");
    }
#else
    if (!ranTerm) {
        FAIL("didn't run term");
    }
#endif

    PASS("Success");
}

