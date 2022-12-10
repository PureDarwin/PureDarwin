// BUILD:  $CC fooimpl.c -dynamiclib -o $BUILD_DIR/libfooimpl.dylib -install_name $RUN_DIR/libfooimpl.dylib
// BUILD:  $CC foo.c -dynamiclib $BUILD_DIR/libfooimpl.dylib -o $BUILD_DIR/libfoo.dylib -Wl,-interposable_list,$SRC_DIR/interposable.txt -install_name $RUN_DIR/libfoo.dylib
// BUILD:  $CC bar.c -bundle     $BUILD_DIR/libfooimpl.dylib -o $BUILD_DIR/libbar.bundle -Wl,-interposable_list,$SRC_DIR/interposable.txt
// BUILD:  $CC main.c -DRUN_DIR="$RUN_DIR" -Wl,-interposable_list,$SRC_DIR/interposable.txt -o $BUILD_DIR/interpose-then-dlopen.exe
// BUILD:  $DYLD_ENV_VARS_ENABLE $BUILD_DIR/interpose-then-dlopen.exe
// BUILD:  $CC interposer.c -dynamiclib $BUILD_DIR/libfooimpl.dylib -o $BUILD_DIR/libinterposer.dylib -install_name libinterposer.dylib

// RUN:    DYLD_INSERT_LIBRARIES=libinterposer.dylib   ./interpose-then-dlopen.exe



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "test_support.h"

// Note, libinterposer.dylib interposes interposableFoo
extern int interposableFoo();

// Note, libfoo interposes interposableBar
extern int interposableBar();

extern int callFunc();


static void tryImage(const char* path, int expectedFoo, int expectedBar)
{
  void* handle = dlopen(path, RTLD_LAZY);
  if ( handle == NULL ) {
    FAIL("dlopen(\"%s\") error: %s", path, dlerror());
  }
  
  __typeof(&callFunc) callFooSym = (__typeof(&callFunc))dlsym(handle, "callFoo");
  if ( callFooSym == NULL ) {
    FAIL("dlsym(\"callFoo\") error: %s", dlerror());
  }
  
  int fooResult = callFooSym();
  if ( fooResult != expectedFoo ) {
    FAIL("callFoo() from %s not interposed as it returned %d", path, fooResult);
  }
  
  __typeof(&callFunc) callBarSym = (__typeof(&callFunc))dlsym(handle, "callBar");
  if ( callBarSym == NULL ) {
    FAIL("dlsym(\"callBar\") error: %s", dlerror());
  }
  
  int barResult = callBarSym();
  if ( barResult != expectedBar ) {
    FAIL("callBar() from %s not interposed as it returned %d", path, barResult);
  }
  
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    tryImage(RUN_DIR "/libfoo.dylib", 4, 2);
    tryImage(RUN_DIR "/libbar.bundle", 4, 100);

    PASS("Success");
}
