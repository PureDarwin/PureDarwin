
// This tests that our header such as dlfcn.h pass unix conformance.

// BUILD(macos):  $CC main.c -o $BUILD_DIR/unix-conformance.exe -D_XOPEN_SOURCE=600
// BUILD(macos):  $CC main.c -o $BUILD_DIR/scratch.exe -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200112
// BUILD(macos): $SKIP_INSTALL $BUILD_DIR/scratch.exe

// BUILD(ios,tvos,watchos,bridgeos):

// RUN:  ./unix-conformance.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <dlfcn.h> 

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    PASS("Success");
}

