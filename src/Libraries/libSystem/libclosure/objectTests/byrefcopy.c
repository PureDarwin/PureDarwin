/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  byrefcopy.m
//  testObjects
//
//  Created by Blaine Garst on 5/13/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CONFIG

#include <stdio.h>
#include <Block.h>
#include <Block_private.h>
#include "test.h"

void callVoidVoid(void (^closure)(void)) {
    closure();
}

int main() {
    int __block i = 10;
    
    void (^block)(void) = ^{ ++i; };
    //printf("original (old style) is  %s\n", _Block_dump_old(block));
    //printf("original (new style) is %s\n", _Block_dump(block));
    void (^blockcopy)(void) __unused = Block_copy(block);
    //printf("copy is %s\n", _Block_dump(blockcopy));
    // use a copy & see that it updates i
    callVoidVoid(block);
    
    if (i != 11) {
        fail("didn't update i");
        return 1;
    }

    succeed(__FILE__);
}
