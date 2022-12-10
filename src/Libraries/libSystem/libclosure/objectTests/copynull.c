/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  copynull.c
 *  testObjects
 *
 *  Created by Blaine Garst on 10/15/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

// TEST_CONFIG

// rdar://6295848
 
#import <stdio.h>
#import <Block.h>
#import <Block_private.h> 
#import "test.h"

int main() {
    
    void (^block)(void) = (void (^)(void))0;
    void (^blockcopy)(void) = Block_copy(block);
    
    if (blockcopy != (void (^)(void))0) {
        fail("whoops, somehow we copied NULL!");
    }
    // make sure we can also
    Block_release(blockcopy);
    // and more secretly
    //_Block_destroy(blockcopy);

    succeed(__FILE__);
}
