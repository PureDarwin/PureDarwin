
// BUILD:  $CXX foo.cpp -dynamiclib -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib -fno-exceptions
// BUILD:  $CXX bar.cpp -dynamiclib -install_name $RUN_DIR/libbar.dylib -o $BUILD_DIR/libbar.dylib -fno-exceptions
// BUILD:  $CXX main.cpp  $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/weak-coalesce-inserted-dylibs.exe -fno-exceptions
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/weak-coalesce-inserted-dylibs.exe

// RUN: DYLD_INSERT_LIBRARIES=libbar.dylib ./weak-coalesce-inserted-dylibs.exe


#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"

extern void foo();

// We have our own copy of operator new.  Make sure that we call our version, but that foo calls the one from the inserted bar dylib
static bool calledMainNew = false;
static bool calledMainDelete = false;
static bool enableTracking = false;

void* operator new(size_t size)
{
    if (enableTracking)
        calledMainNew = true;
    void* ptr = malloc(size);
    return ptr;
}

void operator delete(void* ptr)
{
    if (enableTracking)
        calledMainDelete = true;
    free(ptr);
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // First make sure we do use our versions of new and delete.
    enableTracking = true;

    int* v = new int(1);
    if (!calledMainNew) {
        FAIL("Didn't call executable operator new");
    }

    delete v;
    if (!calledMainDelete) {
        FAIL("Didn't call executable operator delete");
    }

    // Now make foo do the same and make sure we got the new/delete from bar
    calledMainNew = false;
    calledMainDelete = false;
    foo();

    if (calledMainNew) {
        FAIL("Didn't call bar operator new");
    }

    if (calledMainDelete) {
        FAIL("Didn't call bar operator delete");
    }

    PASS("Success");
}

