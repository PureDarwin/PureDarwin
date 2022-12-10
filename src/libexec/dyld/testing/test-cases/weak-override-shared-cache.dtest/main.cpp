
// BUILD:  $CC main.cpp -lc++ -o $BUILD_DIR/weak-override-shared-cache.exe

// RUN:  ./weak-override-shared-cache.exe


#include <stdexcept>
#include <stdio.h>

// Hack to get a strong definition of this symbol
__attribute__((used))
void* hack __asm("__ZTISt16nested_exception");

#include "test_support.h"

int main(int argc, const char* argv[], const char* envp[], const char* apple[]) {
    try {
        throw new std::nested_exception();
    } catch (std::nested_exception* e) {
        PASS("Success");
    }
    FAIL("Expected exception to be thrown");
}

