/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  refcounting.m
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import <Block.h>
#import <Block_private.h>
#import "test.h"

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int i = 10;
    void (^blockA)(void) = ^ { printf("i is %d\n", i); };

    // make sure we can retain/release it
    for (int i = 0; i < 1000; ++i) {
        [blockA retain];
    }
    for (int i = 0; i < 1000; ++i) {
        [blockA release];
    }
    // smae for a copy
    void (^blockAcopy)(void) = [blockA copy];
    for (int i = 0; i < 1000; ++i) {
        [blockAcopy retain];
    }
    for (int i = 0; i < 1000; ++i) {
        [blockAcopy release];
    }
    [blockAcopy release];
    // now for the other guy
    blockAcopy = Block_copy(blockA);
        
    for (int i = 0; i < 1000; ++i) {
        void (^blockAcopycopy)(void) = Block_copy(blockAcopy);
        if (blockAcopycopy != blockAcopy) {
            fail("copy %p of copy %p wasn't the same!!", (void *)blockAcopycopy, (void *)blockAcopy);
        }
    }
    for (int i = 0; i < 1000; ++i) {
        Block_release(blockAcopy);
    }
    Block_release(blockAcopy);
    [pool drain];

    succeed(__FILE__);
}
