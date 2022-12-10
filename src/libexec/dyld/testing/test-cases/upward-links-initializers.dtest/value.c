#include "test_support.h"

static int initValue = 0;

void checkInitOrder(int expected)
{
    initValue++;
    if ( initValue != expected)
            FAIL("Wrong init value in bar: %d, should have been %d.", initValue, expected);
}
