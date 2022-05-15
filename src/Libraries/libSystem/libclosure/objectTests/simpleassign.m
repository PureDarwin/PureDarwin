/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  simpleassign.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CONFIG


#import <Foundation/Foundation.h>
#import "test.h"

int main() {
    id aBlock;
    void (^blockA)(void) = ^ { printf("hello\n"); };
    // a block should be assignable to an id
    aBlock = blockA;
    blockA = aBlock;

    succeed(__FILE__);
}
