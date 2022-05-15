/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG rdar://6396238

#include <stdio.h>
#include <stdlib.h>
#include "test.h"

static int count = 0;

void (^mkblock(void))(void)
{
    count++;
    return ^{
        count++;
    };
}

int main() {
    mkblock()();
    if (count != 2) {
        fail("failure, 2 != %d\n", count);
    }

    succeed(__FILE__);
}
