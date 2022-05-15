/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  byrefgc.m
//  testObjects
//
//  Created by Blaine Garst on 5/16/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CFLAGS -framework Foundation


#import <stdio.h>
#import <Block.h>
#import "test.h"
#import "testroot.i"

int GotHi = 0;

int VersionCounter = 0;

@interface TestObject : TestRoot {
    int version;
}
- (void) hi;
@end

@implementation TestObject


- (id)init {
    version = VersionCounter++;
    return self;
}

- (void) hi {
    GotHi++;
}

@end


void (^get_block(void))(void) {
    __block TestObject * to = [[TestObject alloc] init];
    return [^{ [to hi]; to = [[TestObject alloc] init]; } copy];
}

int main() {
    
    void (^voidvoid)(void) = get_block();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid();
    voidvoid();
    RELEASE_VAR(voidvoid);
    testprintf("alloc %d dealloc %d\n", TestRootAlloc, TestRootDealloc);
#if __has_feature(objc_arc)
    // one TestObject still alive in get_block's __block variable
    testassert(TestRootAlloc == TestRootDealloc + 1);
#else
    // __block variables are unretained so they all leaked
    testassert(TestRootAlloc == 7);
    testassert(TestRootDealloc == 0);
#endif

    succeed(__FILE__);
}
