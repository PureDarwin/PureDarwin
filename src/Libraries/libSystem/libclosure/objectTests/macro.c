/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG 
// rdar://6718399

#include <stdio.h>
#include <Block.h>
#include "test.h"

int main() {
    void (^bbb)(void) __unused = Block_copy(^ {
        int j __unused, cnt __unused;
    });

    succeed(__FILE__);
}
