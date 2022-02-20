// BUILD:  $CC foo.c -dynamiclib -o $BUILD_DIR/libfoo.dylib -install_name $RUN_DIR/libfoo.dylib -Wl,@$SRC_DIR/extra.cmds
// BUILD:  $CC main.c $BUILD_DIR/libfoo.dylib -o $BUILD_DIR/large-load-commands.exe

// RUN:    ./large-load-commands.exe



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    PASS("Success");
}
