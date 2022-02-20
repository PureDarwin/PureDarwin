
// BUILD:  $CC bar.c -dynamiclib -o $BUILD_DIR/door1/libbar.dylib -install_name $RUN_DIR/libbar.dylib -DVALUE=3
// BUILD:  $CC bar.c -dynamiclib -o $BUILD_DIR/door2/libbar.dylib -install_name $RUN_DIR/libbar.dylib -DVALUE=17
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/door1/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib -DVALUE=10  $BUILD_DIR/door1/libbar.dylib
// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/door2/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib -DVALUE=25  $BUILD_DIR/door2/libbar.dylib
// BUILD:  $CC main.c            -o $BUILD_DIR/dlopen-DYLD_LIBRARY_PATH.exe 
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/dlopen-DYLD_LIBRARY_PATH.exe

// RUN:  DYLD_LIBRARY_PATH=$RUN_DIR/door1/                  ./dlopen-DYLD_LIBRARY_PATH.exe  13
// RUN:  DYLD_LIBRARY_PATH=$RUN_DIR/door2                   ./dlopen-DYLD_LIBRARY_PATH.exe  42
// RUN:  DYLD_LIBRARY_PATH=$RUN_DIR/door3/:$RUN_DIR/door2/  ./dlopen-DYLD_LIBRARY_PATH.exe  42

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "test_support.h"

// Program dlopen()s libfoo.dylib which was linked against libbar.dylib
// Neither have valid paths and must be found via DYLD_LIBRARY_PATH
// This test direct and indirect loading.

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    const char* env = getenv("DYLD_LIBRARY_PATH");
    if ( env == NULL ) {
        FAIL("env not set");
    }
    const char* valueStr = argv[1];
    if ( valueStr == NULL ) {
        FAIL("arg1 value not set");
    }
    char* end;
    long value = strtol(valueStr, &end, 0);


    void* handle = dlopen("/bogus/libfoo.dylib", RTLD_LAZY);
    if ( handle == NULL ) {
        FAIL("dlerror(\"/bogus/libfoo.dylib\"): %s", dlerror());
    }

    typedef int (*FooProc)();

    FooProc sym = (FooProc)dlsym(handle, "foo");
    if ( sym == NULL ) {
        FAIL("dlerror(): %s", dlerror());
    }

    int result = (*sym)();
    if ( result != value ) {
        FAIL("result=%d, expected %ld (str=%s)", result, value, valueStr);
    }

	int r = dlclose(handle);
	if ( r != 0 ) {
        FAIL("dlclose() returned %d", r);
    }

    void* handle2 = dlopen("/junk/libfoo.dylib", RTLD_LAZY);
    if ( handle2 == NULL ) {
        FAIL("dlerror(\"/junk/libfoo.dylib\"): %s", dlerror());
    }

    PASS("Success");
}

