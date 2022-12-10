/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  simplemessage.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CONFIG

#import <Foundation/Foundation.h>
#import "test.h"

int main() {
    void (^blockA)(void) = ^ { abort(); };
    // a block be able to be sent a message
    if (*(int *)(void *)blockA == 0x12345) [blockA copy];

    succeed(__FILE__);
}
