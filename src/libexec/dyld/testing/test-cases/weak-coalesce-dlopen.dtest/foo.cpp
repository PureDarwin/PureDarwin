
#include <new>

extern "C" void* foo() {
    return new int(1);
}
