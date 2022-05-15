/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG

#include <Block.h>
#include <stdio.h>
#include "test.h"

// rdar://6225809
// fixed in 5623

int main() {
    __block int a = 42;
    int* ap = &a; // just to keep the address on the stack.

    void (^b)(void) = ^{
        //a;              // workaround, a should be implicitly imported
        (void)Block_copy(^{
            a = 2;
        });
    };

    (void)Block_copy(b);

    if(&a == ap) {
        fail("__block heap storage should have been created at this point");
    }
    
    succeed(__FILE__);
}
