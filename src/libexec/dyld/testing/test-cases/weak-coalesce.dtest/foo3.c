#include "test_support.h"

#include "base.h"

int __attribute__((weak))	coal1 = 3;
int __attribute__((weak))	coal2 = 2;

static __attribute__((constructor))
void myinit(int argc, const char* argv[], const char* envp[], const char* apple[])
{
    LOG("myinit() in foo1.c");
    baseVerifyCoal1("in foo3", &coal1);
    baseVerifyCoal2("in foo3", &coal2);
}

