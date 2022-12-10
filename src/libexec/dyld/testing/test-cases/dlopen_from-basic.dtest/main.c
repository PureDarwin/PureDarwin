
// BUILD:  $CC bar.c  -dynamiclib -install_name @rpath/libbar.dylib -o $BUILD_DIR/dir/libbar.dylib
// BUILD:  $CC bar.c  -dynamiclib -install_name @rpath/libbaz.dylib -o $BUILD_DIR/dir/libbaz.dylib
// BUILD:  $CC test.c -dynamiclib -install_name $RUN_DIR/test/libtest.dylib -o $BUILD_DIR/test/libtest.dylib -rpath @loader_path/../dir
// BUILD:  $CC main.c -o $BUILD_DIR/dlopen_from-basic.exe $BUILD_DIR/test/libtest.dylib

// RUN:  ./dlopen_from-basic.exe

#include <stdio.h>
#include <dlfcn_private.h>
#include <stdbool.h>

#include "test_support.h"

/// test dlopen_from() works for both @rpath and @loader_path

extern void test();

int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    // verify the straight dlopen fails because libtest.dylib sets up the LC_RPATH
    void* handle = dlopen("@rpath/libbar.dylib", RTLD_LAZY);
    if ( handle != NULL ) {
        FAIL("dlopen(\"@rpath/libbar.dylib\") should not have succeeded");
    }
    // verify dlopen_from() works becuase libtest.dylib sets up an LC_RPATH
    handle = dlopen_from("@rpath/libbar.dylib", RTLD_LAZY, &test);
    if ( handle == NULL ) {
        FAIL("dlopen_from(\"@rpath/libbar.dylib\", &test) failed: %s", dlerror());
    }

    // verify the straight dlopen fails because main executables @loader_path is used
    handle = dlopen("@loader_path/../dir/libbaz.dylib", RTLD_LAZY);
    if ( handle != NULL ) {
        FAIL("dlopen(\"@loader_path/../dir/libbaz.dylib\") should not have succeeded");
    }
    // verify dlopen_from() works with @loader_path resolving to where libtest.dylib is
    handle = dlopen_from("@loader_path/../dir/libbaz.dylib", RTLD_LAZY, &test);
    if ( handle == NULL ) {
        FAIL("dlopen_from(\"@loader_path/../dir/libbaz.dylib\", &test) failed: %s", dlerror());
    }

    PASS("dlopen_from");
}

