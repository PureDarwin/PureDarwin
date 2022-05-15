/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  copytest.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import <Block_private.h>
#import "test.h"

int GlobalInt = 0;
void setGlobalInt(int value) { GlobalInt = value; }

int main(int argc __unused, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int y = 0;
    // must use x+y to avoid optimization of using a global block
    void (^callSetGlobalInt)(int x) = ^(int x) { setGlobalInt(x + y); };
    // a block be able to be sent a message
    void (^callSetGlobalIntCopy)(int) = [callSetGlobalInt copy];
    if (callSetGlobalIntCopy == callSetGlobalInt) {
        // testwarn("copy looks like: %s", _Block_dump(callSetGlobalIntCopy));
        fail("copy is identical", argv[0]);
    }
    callSetGlobalIntCopy(10);
    if (GlobalInt != 10) {
        fail("copy did not set global int");
    }
    [pool drain];

    succeed(__FILE__);
}
