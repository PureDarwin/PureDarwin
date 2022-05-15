/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG

#import <stdio.h>
#import <Block.h>
#import "test.h"

int global;

void (^gblock)(int) = ^(int x){ global = x; };

int main() {
    gblock(1);
    if (global != 1) {
        fail("did not set global to 1");
    }
    void (^gblockcopy)(int) = Block_copy(gblock);
    if (gblockcopy != gblock) {
        fail("global copy %p not a no-op %p", (void *)gblockcopy, (void *)gblock);
    }
    Block_release(gblockcopy);
    gblock(3);
    if (global != 3) {
        fail("did not set global to 3");
    }
    gblockcopy = Block_copy(gblock);
    gblockcopy(5);
    if (global != 5) {
        fail("did not set global to 5");
    }

    succeed(__FILE__);
}

