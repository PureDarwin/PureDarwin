
// BUILD:  $CC main.c -o $BUILD_DIR/dlopen-dyld.exe

// RUN:  ./dlopen-dyld.exe


#include <stdbool.h>
#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"


int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    void* handle = dlopen("/usr/lib/dyld", RTLD_LAZY);
    if ( handle == NULL )
        PASS("Success");
    else
        FAIL("dlopen-dyld: dlopen() should have failed");
}
