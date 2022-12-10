
// BUILD:  $CXX main.cxx -o $BUILD_DIR/dyld_register_test.exe -DRUN_DIR="$RUN_DIR"
// BUILD:  $CC  foo.c -dynamiclib  -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib

// RUN:  ./dyld_register_test.exe

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>

#include <unordered_set>

#include "test_support.h"

extern mach_header __dso_handle;

static std::unordered_set<const mach_header*> sCurrentImages;

static void notify(const mach_header* mh, intptr_t vmaddr_slide)
{
    LOG("mh=%p", mh);
    if ( sCurrentImages.count(mh) != 0 ) {
        FAIL("notified twice about %p", mh);
    }
    sCurrentImages.insert(mh);
}


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    _dyld_register_func_for_add_image(&notify);

    // verify we were notified about already loaded images
    if ( sCurrentImages.count(&__dso_handle) == 0 ) {
        FAIL("did not notify us about main executable");
    }
    const mach_header* libSysMH = dyld_image_header_containing_address((void*)&printf);
    if ( sCurrentImages.count(libSysMH) == 0 ) {
        FAIL("did not notify us about libsystem_c.dylib");    }

    void* handle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_FIRST);
    if ( handle == NULL ) {
        FAIL("dlopen(\"%s\") failed with: %s", RUN_DIR "/libfoo.dylib", dlerror());
    }

    void* sym = dlsym(handle, "foo");
    if ( sym == NULL ) {
        FAIL("dlsym(handle, \"foo\") failed");
    }

    // verify we were notified about load of libfoo.dylib
    const mach_header* libfooMH = dyld_image_header_containing_address(sym);
    if ( sCurrentImages.count(libfooMH) == 0 ) {
		FAIL("_dyld_register_func_for_add_image() did not notify us about libfoo.dylib");
    }

	int result = dlclose(handle);
	if ( result != 0 ) {
        FAIL("dlclose(handle) returned %d", result);
	}

    PASS("_dyld_register_func_for_add_image");
}

