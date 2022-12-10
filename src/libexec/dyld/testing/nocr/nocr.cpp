#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach.h>

#include "test_support.h"

extern const char**  environ;

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    if ( argc < 2 ) {
        fprintf(stderr, "usage: nocr prog args...\n");
        return EXIT_FAILURE;
    }
    
    _process process;
    process.set_executable_path(argv[1]);
    process.set_args(&argv[2]);
    process.set_env(environ);
    process.set_crash_handler(^(task_t task) {
        exit(0);
    });
    process.set_exit_handler(^(pid_t pid) {
        int status = 0;
        (void)waitpid(pid, &status, 0);
        
        // Only call exit if the child exited normally, otherwise keep running to consume the crash
        if (WIFEXITED(status)) {
            exit(0);
        }
    });
    process.launch();
    dispatch_main();
}
