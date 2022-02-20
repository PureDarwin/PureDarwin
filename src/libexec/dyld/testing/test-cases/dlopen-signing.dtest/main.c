// BUILD:  $CC dylib.c -dynamiclib -o $BUILD_DIR/signed.dylib
// BUILD:  $CC dylib.c -dynamiclib -o $BUILD_DIR/unsigned.dylib
// BUILD:  $CC main.c            -o $BUILD_DIR/dlopen-signed.exe
// BUILD:  $CC main.c            -o $BUILD_DIR/dlopen-unsigned.exe

// FIXME: add builds that sign the executable and the dylib in in various ways
// At this time we don't have a way to do that, so this test must be run
// manually.

#include <stdio.h>
#include <dlfcn.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
  void* handle = dlopen("signed.dylib", RTLD_LAZY);
  if ( handle == NULL ) {
     FAIL("dlerror(): %s", dlerror());
  } else {
    int result = dlclose(handle);
    if ( result != 0 ) {
       FAIL("dlclose() returned %c", result);
    }
  }

  handle = dlopen("unsigned.dylib", RTLD_LAZY);
  if ( handle != NULL ) {
     FAIL("dlerror(): %s", dlerror());
  } else {
    int result = dlclose(handle);
    if ( result != 0 ) {
       FAIL("dlclose() returned %c", result);
    }
  }

  PASS("Success");
}


