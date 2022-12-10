/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG LANGUAGE=c,objc
// TEST_CFLAGS -std=c99 -fblocks

//
//  c99.m
//
// rdar://problem/6399225

#import <stdio.h>
#import <stdlib.h>
#import "test.h"

int main() {
    void (^blockA)(void) = ^ { ; };
    blockA();

    succeed(__FILE__);
}
