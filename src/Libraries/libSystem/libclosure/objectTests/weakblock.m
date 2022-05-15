/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  weakblock.m
//  testObjects
//
//  Created by Blaine Garst on 10/30/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation
//
// rdar://5847976
// Super basic test - does compiler a) compile and b) call out on assignments

#import <Foundation/Foundation.h>
#import "test.h"

// provide our own version for testing

int GotCalled = 0;

void _Block_object_assign(void *destAddr, const void *object, const int flags) {
    printf("_Block_object_assign(dest %p, value %p, flags %x)\n", destAddr, object, flags);
    ++GotCalled;
}

int recovered = 0;

@interface TestObject : NSObject {
}
@end

@implementation TestObject
- (id)retain {
    fail("Whoops, retain called!");
}
- (void)dealloc {
    ++recovered;
    [super dealloc];
}
@end


void testRR() {
    // create test object
    TestObject *to = [[TestObject alloc] init];
    __block TestObject *__weak  testObject __unused = to;    // iniitialization does NOT require support function
}

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    GotCalled = 0;
    testRR();
    if (GotCalled == 1) {
        fail("called out to support function on initialization");
        return 1;
    }
    [pool drain];

    succeed(__FILE__);
}
