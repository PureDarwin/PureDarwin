/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  blockimport.c
 *  testObjects
 *
 *  Created by Blaine Garst on 10/13/08.
//  Copyright 2008 Apple, Inc. All rights reserved.
 *
 */


// rdar://6289344
// TEST_CONFIG

#include <stdio.h>
#include <Block.h>
#include <Block_private.h>
#include "test.h"

int main() {
    int i = 1;
    int (^intblock)(void) = ^{ return i*10; };
    
    void (^vv)(void) = ^{
        testprintf("intblock returns %d\n", intblock());
    };

    // printf("Block dump %s\n", _Block_dump(vv));

    void (^vvcopy)(void) = Block_copy(vv);
    Block_release(vvcopy);
    
    succeed(__FILE__);
}
