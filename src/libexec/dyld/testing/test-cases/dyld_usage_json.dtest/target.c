#include <dlfcn.h>
#include <unistd.h>

#include "test_support.h"
int main(int argc, const char* argv[])
{
    void* handle = dlopen(RUN_DIR "/libfoo.dylib", RTLD_NOW);
    void *foo = dlsym(handle, "foo");
    dlclose(handle);
    return 0;
}
