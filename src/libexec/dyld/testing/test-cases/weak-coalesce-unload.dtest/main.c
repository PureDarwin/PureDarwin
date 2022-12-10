
// BUILD:  $CC foo1.c -dynamiclib -install_name $RUN_DIR/libfoo1.dylib -o $BUILD_DIR/libfoo1.dylib
// BUILD:  $CC foo2.c -dynamiclib -install_name $RUN_DIR/libfoo2.dylib -o $BUILD_DIR/libfoo2.dylib
// BUILD:  $CC foo3.c -dynamiclib -install_name $RUN_DIR/libfoo3.dylib -o $BUILD_DIR/libfoo3.dylib
// BUILD:  $CC main.c -o $BUILD_DIR/weak-coalesce-unload.exe -DRUN_DIR="$RUN_DIR"

// RUN:  ./weak-coalesce-unload.exe


#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "test_support.h"

extern int foo();
extern void* fooPtr();

int main()
{
	// dlopen foo1 which defines "foo"
	void* handle1 = dlopen(RUN_DIR "/libfoo1.dylib", RTLD_FIRST);
    if ( handle1 == NULL ) {
        FAIL("dlopen(\"%s\") failed with: %s", RUN_DIR "/libfoo1.dylib", dlerror());
    }

    const void* symFoo1 = dlsym(handle1, "foo");
    if ( symFoo1 == NULL ) {
        FAIL("dlsym(handle1, foo) failed");
    }

    const void* symFooPtr1 = dlsym(handle1, "fooPtr");
    if ( symFooPtr1 == NULL ) {
        FAIL("dlsym(handle1, fooPtr) failed");
    }
    void* fooptr1 = ((__typeof(&fooPtr))symFooPtr1)();

    int close1 = dlclose(handle1);
    if ( close1 != 0 ) {
        FAIL("dlclose(handle1) failed with: %s", dlerror());
    }

    // Now dlopen foo2 and get the value it finds for foo
    void* handle2 = dlopen(RUN_DIR "/libfoo2.dylib", RTLD_FIRST);
    if ( handle2 == NULL ) {
        FAIL("dlopen(\"%s\") failed with: %s", RUN_DIR "/libfoo2.dylib", dlerror());
    }

    const void* symFoo2 = dlsym(handle2, "foo");
    if ( symFoo2 == NULL ) {
        FAIL("dlsym(handle2, foo) failed");
    }

    const void* symFooPtr2 = dlsym(handle2, "fooPtr");
    if ( symFooPtr2 == NULL ) {
        FAIL("dlsym(handle2, fooPtr) failed");
    }
    void* fooptr2 = ((__typeof(&fooPtr))symFooPtr2)();

    // Don't close foo2, but instead open foo3
    void* handle3 = dlopen(RUN_DIR "/libfoo3.dylib", RTLD_FIRST);
    if ( handle3 == NULL ) {
        FAIL("dlopen(\"%s\") failed with: %s", RUN_DIR "/libfoo3.dylib", dlerror());
    }

    const void* symFoo3 = dlsym(handle3, "foo");
    if ( symFoo3 == NULL ) {
        FAIL("dlsym(handle3, foo) failed");
    }

    const void* symFooPtr3 = dlsym(handle3, "fooPtr");
    if ( symFooPtr3 == NULL ) {
        FAIL("dlsym(handle3, fooPtr) failed");
    }
    void* fooptr3 = ((__typeof(&fooPtr))symFooPtr3)();

    // No-one should point to libfoo1.dylib
    if ( symFoo1 == symFoo2 ) {
    	FAIL("foo1 == foo2");
    }
    if ( symFoo1 == symFoo3 ) {
    	FAIL("foo1 == foo3");
    }

    // foo2 and foo3 should be different
    if ( symFoo2 == symFoo3 ) {
    	FAIL("foo2 != foo3");
    }

    // But their coalesced values should be the same
    if ( fooptr1 == fooptr2 ) {
    	FAIL("fooptr1 == fooptr2");
    }
    if ( fooptr2 != fooptr3 ) {
    	FAIL("fooptr2 != fooptr3");
    }

    // foo should return the value from foo2, not the value from foo3
    // Also calling this would explode if we somehow pointed at foo1
    if ( ((__typeof(&foo))fooptr2)() != 2 ) {
    	FAIL("foo2 != 2");
    }
    if ( ((__typeof(&foo))fooptr3)() != 2 ) {
    	FAIL("foo3 != 2");
    }

    PASS("weak-coalesce-unload");
}

