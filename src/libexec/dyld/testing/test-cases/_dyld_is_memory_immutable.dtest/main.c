
// BUILD:  $CC foo.c -dynamiclib  -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC bar.c -dynamiclib  -install_name $RUN_DIR/libbar.dylib -o $BUILD_DIR/libbar.dylib
// BUILD:  $CC main.c -DRUN_DIR="$RUN_DIR" $BUILD_DIR/libfoo.dylib    -o $BUILD_DIR/dyld_immutable_test.exe

// RUN:  ./dyld_immutable_test.exe

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>

#if __has_feature(ptrauth_calls)
    #include <ptrauth.h>
#endif

#include "test_support.h"

static const void* stripPointer(const void* ptr)
{
#if __has_feature(ptrauth_calls)
    return __builtin_ptrauth_strip(ptr, ptrauth_key_asia);
#else
    return ptr;
#endif
}


typedef const char* (*BarProc)(void);

extern uint32_t _cpu_capabilities;
extern const char* foo();

const char* myStr = "myStr";

int myInt;


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( !_dyld_is_memory_immutable(myStr, 6) ) {
        FAIL("returned false for string in main executable");
    }

    if ( _dyld_is_memory_immutable(strdup("hello"), 6) ) {
        FAIL("returned true for result from strdup()");
    }

    if ( _dyld_is_memory_immutable(&myInt, 4) ) {
        FAIL("returned true for global variabe in main executable");
    }

    if ( !_dyld_is_memory_immutable(foo(), 4) ) {
        FAIL("returned false for string in statically linked dylib");
    }

    if ( !_dyld_is_memory_immutable(stripPointer((void*)&strcpy), 4) ) {
        FAIL("returned false for strcpy function in dyld shared cache");
    }

    if ( _dyld_is_memory_immutable(&_cpu_capabilities, 4) ) {
        FAIL("returned true for global variable in shared cache");
    }

    void* handle = dlopen(RUN_DIR "/libbar.dylib", RTLD_FIRST);
    if ( handle == NULL ) {
        FAIL("dlopen(libbar.dylib) failed");    }

    BarProc proc = dlsym(handle, "bar");
    if ( proc == NULL ) {
        FAIL("dlsym(bar) failed");
    }
    const char* barStr = (*proc)();
    if ( _dyld_is_memory_immutable(barStr, 4) ) {
        FAIL("returned true for string in unloadable dylib");
    }

    PASS("Success");
}

