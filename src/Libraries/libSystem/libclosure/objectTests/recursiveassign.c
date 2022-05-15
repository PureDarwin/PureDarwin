/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  recursiveassign.c
 *  testObjects
 *
 *  Created by Blaine Garst on 12/3/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

// TEST_CONFIG rdar://6639533

// The compiler is prefetching x->forwarding before evaluting code that recomputes forwarding and so the value goes to a place that is never seen again.

#include <stdio.h>
#include <stdlib.h>
#include <Block.h>
#include "test.h"

int main() {
    
    __block void (^recursive_copy_block)(int) = ^(int arg __unused) { 
        fail("got wrong Block"); 
    };
    __block int done = 2;
    
    recursive_copy_block = Block_copy(^(int i) {
        if (i > 0) {
            recursive_copy_block(i - 1);
        }
        else {
            if (done != 0) abort();
            done = 1;
        }
    });
    
    done = 0;
    recursive_copy_block(5);
    testassert(done == 1);
    
    succeed(__FILE__);
}

