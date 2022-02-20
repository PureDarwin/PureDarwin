#include "test_support.h"

__attribute__((constructor))
void init(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    FAIL("Bad dylib loaded");
}
