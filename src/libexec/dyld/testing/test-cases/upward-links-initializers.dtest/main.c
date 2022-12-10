// BUILD:  $CC value.c -dynamiclib -install_name $RUN_DIR/libvalue.dylib  -o $BUILD_DIR/libvalue.dylib
// BUILD:  $CC init4.c -dynamiclib -install_name $RUN_DIR/libinit4.dylib  -o $BUILD_DIR/libinit4.dylib $BUILD_DIR/libvalue.dylib
// BUILD:  $CC init5.c -dynamiclib -install_name $RUN_DIR/libinit5.dylib  -o $BUILD_DIR/libinit5.dylib $BUILD_DIR/libinit4.dylib $BUILD_DIR/libvalue.dylib
// BUILD:  $CC init6.c -dynamiclib -install_name $RUN_DIR/libinit6.dylib  -o $BUILD_DIR/libinit6.dylib $BUILD_DIR/libinit5.dylib $BUILD_DIR/libvalue.dylib
// BUILD:  $CC init1.c -dynamiclib -install_name $RUN_DIR/libinit1.dylib  -o $BUILD_DIR/libinit1.dylib -Wl,-upward_library,$BUILD_DIR/libinit6.dylib $BUILD_DIR/libvalue.dylib
// BUILD:  $CC init2.c -dynamiclib -install_name $RUN_DIR/libinit2.dylib  -o $BUILD_DIR/libinit2.dylib $BUILD_DIR/libinit1.dylib $BUILD_DIR/libvalue.dylib
// BUILD:  $CC init3.c -dynamiclib -install_name $RUN_DIR/libinit3.dylib  -o $BUILD_DIR/libinit3.dylib $BUILD_DIR/libinit2.dylib $BUILD_DIR/libvalue.dylib
// BUILD:  $CC main.c -o $BUILD_DIR/upward-link-initializers.exe -DRUN_DIR="$RUN_DIR" $BUILD_DIR/libinit3.dylib


/*
 *     main ---> libinit3
 *                   |
 *                    -------|
 *        libinit6          libinit2
 *         |    ^            |
 *         |    |            |
 *         |    |            |
 *   libinit5   -----------libinit1
 *      |
 *      |
 *   libinit4
 *
 *   libinit1: lowest direct dependency from top level lib (libinit3)
 *   libinit6: only ever brought via upward link dependency, and should not be initialized before libinit1
 *   libinit4: lowest direct dependency from dangling upward (libinit6)
 */

// RUN:  ./upward-link-initializers.exe


#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[]) {
    // Initializers tests passed
    PASS("Success");
}

