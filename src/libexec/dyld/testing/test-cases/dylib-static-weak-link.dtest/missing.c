#include "test_support.h"

extern int foo __attribute__((weak_import));


int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    // dylib won't be found at runtime, so &foo should be NULL
    if ( &foo == NULL )
        PASS("Success");
    else
        FAIL("&foo != NULL");
}


