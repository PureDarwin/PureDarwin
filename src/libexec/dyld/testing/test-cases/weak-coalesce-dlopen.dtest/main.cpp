
// BUILD:  $CC foo.cpp -lc++ -dynamiclib -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC main.cpp -lc++ -o $BUILD_DIR/weak-coalesce-dlopen.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./weak-coalesce-dlopen.exe


#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <new>

#include "test_support.h"

extern void* foo();

void* lastAllocatedValue = NULL;

void* operator new(size_t size) {
    lastAllocatedValue = malloc(size);
    return lastAllocatedValue;
}

int main()
{
    // The value we allocate should come from our new function
    int* value1 = new int(1);
    if ( value1 != lastAllocatedValue ) {
        FAIL("value1 (%p) != lastAllocatedValue (%p)", value1, lastAllocatedValue);
    }

	// dlopen foo which defines "foo"
    // In dyld2, for chained fixups, this will run weakBindOld which patches the cache for
    // weak defs.  That patching will fail if the cache uses __DATA_CONST and was not marked as
    // RW prior to patching
	void* handle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_FIRST);
    if ( handle == NULL ) {
        FAIL("dlopen(\"%s\") failed with: %s", RUN_DIR "/libfoo.dylib", dlerror());
    }

    const void* symFoo = dlsym(handle, "foo");
    if ( symFoo == NULL ) {
        FAIL("dlsym(handle, foo) failed");
    }

    // The value foo allocates should come from our new function
    void* value2 = ((__typeof(&foo))symFoo)();
    if ( value2 != lastAllocatedValue ) {
        FAIL("value2 (%p) != lastAllocatedValue (%p)", value2, lastAllocatedValue);
    }

    PASS("weak-coalesce-dlopen");
}

