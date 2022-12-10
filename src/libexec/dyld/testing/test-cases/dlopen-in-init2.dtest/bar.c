#include "test_support.h"

static int inited = 0;

__attribute__((constructor))
static void myinit()
{
	inited = 1;
}

int barIsInited() {
	return inited;
}

void bar() {
    if (inited == 0) {
        FAIL("libbar.dylib not initialized");
    }
}
