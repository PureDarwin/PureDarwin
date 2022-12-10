#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dispatch/dispatch.h>

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    signal(SIGUSR1, SIG_IGN);
    dispatch_source_t signalSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, getppid(),
                                                            DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    dispatch_source_set_event_handler(signalSource, ^{
        exit(0);
    });
    dispatch_resume(signalSource);

    dispatch_async(dispatch_get_main_queue(), ^{
            LOG("target starting");
            usleep(1000);
            // load and unload in a loop
            for (int i=1; i < 10000; ++i) {
                void* h = dlopen("./libfoo.dylib", 0);
                usleep(100000/(i*100));
                dlclose(h);
            }
            LOG("target done");
    });

    dispatch_main();
}

