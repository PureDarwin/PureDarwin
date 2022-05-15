/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG rdar://6414583

// a smaller case of byrefcopyint

#include <Block.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include "test.h"

int main() {
    __block int c = 1;

    //printf("&c = %p - c = %i\n", &c, c);

    int i;
    for(i =0; i < 2; i++) {
        dispatch_block_t block = Block_copy(^{ c = i; });

        block();
//        printf("%i: &c = %p - c = %i\n", i, &c, c);

        Block_release(block);
    }

    succeed(__FILE__);
}
