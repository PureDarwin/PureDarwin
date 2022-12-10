#include <dlfcn.h>
#include <pthread.h>

#include "test_support.h"

static void* work1(void* arg)
{
    void* h = dlopen("System/Library/Frameworks/Foundation.framework/Foundation", 0);

    return NULL;
}


__attribute__((constructor))
void myinit(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    pthread_t workerThread;

    if ( pthread_create(&workerThread, NULL, work1, NULL) != 0 ) {
        FAIL("pthread_create");
    }

    void* dummy;
    pthread_join(workerThread, &dummy);
}

