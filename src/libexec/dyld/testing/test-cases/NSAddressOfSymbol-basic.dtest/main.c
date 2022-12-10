// BUILD(macos):  $CC main.c            -o $BUILD_DIR/NSAddressOfSymbol-basic.exe -Wno-deprecated-declarations

// BUILD(ios,tvos,watchos,bridgeos):

// RUN:  ./NSAddressOfSymbol-basic.exe



#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include "test_support.h"

extern struct mach_header __dso_handle;

int patatino(void) {
    return 666;
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    NSSymbol sym = NSLookupSymbolInImage(&__dso_handle, "_main", NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if ( sym == NULL ) {
        FAIL("can't find main");
    }
    void* mainAddr = NSAddressOfSymbol(sym);
    if ( mainAddr != &main ) {
        FAIL("address returned %p is not &main=%p", mainAddr, &main);
    }

    NSSymbol sym2 = NSLookupSymbolInImage(&__dso_handle, "_patatino", NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if ( sym2 == NULL ) {
        FAIL("cant' find patatino");
    }
    void* funcAddr = NSAddressOfSymbol(sym2);
    if ( funcAddr == NULL ) {
        FAIL("address returned for patatino is NULL");
    }
    // This returns a signed pointer, so we want to make sure we can call it without crashing.
    int (*func_ptr)(void) = funcAddr;
    int result = (*func_ptr)();
    if ( result != 666 ) {
        FAIL("can't call the function correctly");
    }

    // verify NULL works
    if ( NSAddressOfSymbol(NULL) != NULL ) {
        FAIL("NULL not handle");
    }

    PASS("Success");
}

