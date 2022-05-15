/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  nestedimport.m
//  testObjects
//
//  Created by Blaine Garst on 6/24/08.
//  Copyright 2008 Apple, Inc. All rights reserved.
//
// TEST_CONFIG 

#include <stdio.h>
#include <stdlib.h>
#include "test.h"

int Global = 0;

void callVoidVoid(void (^closure)(void)) {
    closure();
}

int main(int argc, char **argv __unused) {
    int i = 1;
    
    void (^vv)(void) = ^{
        if (argc > 0) {
            callVoidVoid(^{ Global = i; });
        }
    };
    
    i = 2;
    vv();
    if (Global != 1) {
        fail("Global not set to captured value");
    }

    succeed(__FILE__);
}
