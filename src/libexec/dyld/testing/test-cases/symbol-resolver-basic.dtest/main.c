
// BUILD:  $CC foo.c foo2.c -dynamiclib  -install_name $RUN_DIR/libfoo.dylib -o $BUILD_DIR/libfoo.dylib
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/symbol-resolver.exe
// BUILD:  $CC foo.c foo2.c -dynamiclib -DTEN=1 -install_name $RUN_DIR/libfoo10.dylib -o $BUILD_DIR/libfoo10.dylib 
// BUILD:  $CC main.c $BUILD_DIR/libfoo10.dylib -DTEN=1 -o $BUILD_DIR/symbol-resolver10.exe

// RUN:  ./symbol-resolver.exe
// RUN:  ./symbol-resolver10.exe


#include <stdio.h>
#include <stdlib.h>

#include "test_support.h"

extern int foo();
extern int fooPlusOne();


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
#if TEN
    if ( foo() != 10 )
        FAIL("foo() != 10");
    else if ( fooPlusOne() != 11 )
        FAIL("fooPlusOne() != 11");
    else
        PASS("Success");
#else
    if ( foo() != 0 )
        FAIL("foo() != 0");
    else if ( fooPlusOne() != 1 )
        FAIL("fooPlusOne() != 1");
    else
        PASS("Success");
#endif
  
	return 0;
}
