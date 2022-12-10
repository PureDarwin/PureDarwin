#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_support.h"

static const char* expectedStrings[] = {
    "a() from main",
    "initC",
    "c() from initB",
    "c() from initD",
    "a() from initE",
    "d() from initF",
    "DONE"
};

static const char** curState = expectedStrings;

void setState(const char* from)
{
//    LOG("%s", from);
    if ( strcmp(*curState, from) != 0 ) {
        FAIL("Expected %s", *curState);
    }
    ++curState;
}

