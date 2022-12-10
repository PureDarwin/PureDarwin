
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

#include "test_support.h"

extern int bazInited();

static void* barHandle = NULL;
static void* barSymbol = NULL;
static int fooInited = 0;
static int barInited = 0;

__attribute__((constructor))
static void myinit(int argc, const char* argv[], const char* envp[], const char* apple[]) {
	fooInited = 1;
	barHandle = dlopen(RUN_DIR "/libbar.dylib", 0);
	if ( barHandle == NULL ) {
		FAIL("dlopen libbar.dylib: %s", dlerror());
	}
	barSymbol = dlsym(RTLD_DEFAULT, "barIsInited");
    if ( barSymbol == NULL ) {
		FAIL("dlsym libbar.dylib");
    }
    barInited = ((int(*)())barSymbol)();
}

void foo() {
	if ( fooInited == 0 ) {
		FAIL("Didn't init foo");
	}
	if ( barHandle == NULL ) {
        FAIL("barHandle not inited");
	}
	if ( barSymbol == NULL ) {
        FAIL("barSymbol not inited");
	}
	if ( barInited == 0 ) {
		FAIL("Didn't init bar");
	}
	if ( bazInited() == 0 ) {
		FAIL("Didn't init baz");
	}
}
