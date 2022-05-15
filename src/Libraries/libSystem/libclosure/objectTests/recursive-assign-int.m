/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  recursive-assign-int.m
//  testObjects
//
//  Created by Blaine Garst on 12/4/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CONFIG rdar://6416474

// The compiler is prefetching x->forwarding before evaluting code that recomputes forwarding and so the value goes to a place that is never seen again.

#include <stdio.h>
#include <stdlib.h>
#include <Block.h>
#include "test.h"

typedef void (^blockOfVoidReturningVoid)(void);

blockOfVoidReturningVoid globalBlock;

int nTHCopy(blockOfVoidReturningVoid  block) {
    globalBlock = Block_copy(block);
    return 1;
}

int main() {
    
    __block int x = 0;
    
    x = nTHCopy(^{
        // x should be the value returned by nTHCopy
        if (x != 1) {
            fail("but it wasn't updated properly!");
        }
    });
    
    globalBlock();
    if (x == 0) {
        fail("x here should be 1, but instead is: %d", x);
    }

    succeed(__FILE__);
}

