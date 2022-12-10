/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  byrefaccess.m
//  test that byref access to locals is accurate
//  testObjects
//
//  Created by Blaine Garst on 5/13/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CONFIG

#include <stdio.h>
#include "test.h"

void callVoidVoid(void (^closure)(void)) {
    closure();
}

int main() {
    __block int i = 10;
    
    callVoidVoid(^{ ++i; });
    
    if (i != 11) {
        fail("didn't update i");
        return 1;
    }

    succeed(__FILE__);
}
