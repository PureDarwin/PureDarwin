#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <mach/mach.h>
#include <sys/param.h>
#include <dispatch/dispatch.h>

// The process starts, then sends its parent a SIGUSR1 to indiicate it is ready
// At that point it waits for SIGUSR1, and when it recieves one it loads and unloads libfoo.dylib 3 times
// The process remains running until it recieves a SIGTERM

// This process will clean itself up in the event its parent dies

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // Setup parent death handler
    dispatch_source_t parentDeathSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, getppid(), DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    dispatch_source_set_event_handler(parentDeathSource, ^{
        exit(0);
    });
    dispatch_resume(parentDeathSource);

    // Setup SIGTERM handler
    signal(SIGTERM, SIG_IGN);
    dispatch_source_t exitSignalSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(exitSignalSource, ^{
        exit(0);
    });
    dispatch_resume(exitSignalSource);

    // Setup SIGUSR1 handler
    signal(SIGUSR1, SIG_IGN);
    dispatch_source_t signalSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(signalSource, ^{
        for (int i=0; i < 3; ++i) {
            void* h = dlopen(RUN_DIR "/libfoo.dylib", 0);
            dlclose(h);
        }
        kill(getppid(), SIGUSR2);
    });
    dispatch_resume(signalSource);

    // Message our parent to let them know our signal handlers are ready
    kill(getppid(), SIGUSR1);
    dispatch_main();
}

