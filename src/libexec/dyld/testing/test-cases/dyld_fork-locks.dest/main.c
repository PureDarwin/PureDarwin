
// BUILD:  $CC main.c -o $BUILD_DIR/dyld_fork_test.exe

// RUN:  ./dyld_fork_test.exe 

#include <stdio.h>
#include <dlfcn.h>
#include <mach-o/dyld_priv.h>

#include "test_support.h"

bool isParent = true;

static void notifyBeforeFork(const struct mach_header* mh, intptr_t vmaddr_slide)
{
    static bool sRanNotifier = false;
    if (sRanNotifier)
        return;
    sRanNotifier = true;

    // fork and exec child
    pid_t sChildPid = fork();
    if ( sChildPid < 0 ) {
        FAIL("Didn't fork");
    }
    if ( sChildPid == 0 ) {
        // child side
        isParent = false;
    }
}

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    _dyld_register_func_for_add_image(&notifyBeforeFork);

    if (isParent) {
        PASS("Success");
    }

    return 0;
}
