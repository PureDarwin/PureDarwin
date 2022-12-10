/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG

// rdar://6255170

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <Block.h>
#include <Block_private.h>
#include <assert.h>
#include "test.h"

int main()
{
    __block int var = 0;
    int shouldbe = 0;
    void (^b)(void) = ^{ var++; /*printf("var is at %p with value %d\n", &var, var);*/ };
    __typeof(b) _b;
    //printf("before copy...\n");
    b(); ++shouldbe;
    size_t i;

    for (i = 0; i < 10; i++) {
            _b = Block_copy(b); // make a new copy each time
            assert(_b);
            ++shouldbe;
            _b();               // should still update the stack
            Block_release(_b);
    }

    //printf("after...\n");
    b(); ++shouldbe;

    if (var != shouldbe) {
        fail("var is %d but should be %d", var, shouldbe);
    }

    succeed(__FILE__);
}
