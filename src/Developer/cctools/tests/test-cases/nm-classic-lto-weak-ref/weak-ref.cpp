#include <cstdio>

template<int i>
__attribute__((noinline))
void foo() { printf("%d\n", i); }

void bar() { foo<0>(); }
