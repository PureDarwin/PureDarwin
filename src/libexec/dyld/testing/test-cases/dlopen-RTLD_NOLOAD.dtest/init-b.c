
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

bool doneInitB = false;
bool inInitB = false;


__attribute__((constructor))
void initB(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    inInitB = true;

    // "upward" link to libInitA.dylib
    void* handle = dlopen(RUN_DIR "/libInitA.dylib", RTLD_NOLOAD);
    if ( handle == NULL ) {
        FAIL("dlopen(\"libInitA.dylib\", RTLD_NOLOAD) failed but it should have worked: %s", dlerror());
        return;
    }
    inInitB = false;

    doneInitB = true;
}
