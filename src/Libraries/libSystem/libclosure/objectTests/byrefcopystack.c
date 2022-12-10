/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  byrefcopystack.m
//  testObjects
//
//  Created by Blaine Garst on 5/13/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CONFIG

// rdar://6255170

#include <stdio.h>
#include <Block.h>
#include "test.h"

void (^bumpi)(void);
int (^geti)(void);

void setClosures() {
    int __block i = 10;
    bumpi = Block_copy(^{ ++i; });
    geti = Block_copy(^{ return i; });
}

int main() {
    setClosures();
    bumpi();
    int i = geti();
    
    if (i != 11) {
        fail("didn't update i");
    }

    succeed(__FILE__);
}
