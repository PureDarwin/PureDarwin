
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

#include "test_support.h"

static void* bazHandle = NULL;
static void* bazSymbol = NULL;
static int barInited = 0;
static int bazInited = 0;

__attribute__((constructor))
static void myinit(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    barInited = 1;
    bazHandle = dlopen(RUN_DIR "/libbaz.dylib", 0);
    if ( bazHandle == NULL ) {
        FAIL("dlopen libbaz.dylib: %s", dlerror());
    }
    bazSymbol = dlsym(RTLD_DEFAULT, "bazIsInited");
    if ( bazSymbol == NULL ) {
        FAIL("dlsym libbaz.dylib");
    }
    bazInited = ((int(*)())bazSymbol)();
}

int bar() {
	if ( barInited == 0 ) {
		FAIL("Didn't init bar");
	}
	if ( bazHandle == NULL ) {
        FAIL("bazHandle not inited");
	}
	if ( bazSymbol == NULL ) {
        FAIL("bazSymbol not inited");
	}
	if ( bazInited == 0 ) {
		FAIL("didn't init bar");
	}
	return 0;
}
