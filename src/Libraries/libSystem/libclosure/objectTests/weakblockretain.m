/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  weakblockretain.m
//  testObjects
//
//  Created by Blaine Garst on 11/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation

// rdar://5847976
// Test that weak block variables don't retain/release their contents



#import <Foundation/Foundation.h>
#import <Block.h>
#import "test.h"

int RetainCalled;
int ReleaseCalled;

@interface TestObject : NSObject
@end

@implementation TestObject

- (id)retain {
    RetainCalled = 1;
    return [super retain];
}
- (oneway void)release {
    ReleaseCalled = 1;
    [super release];
}

void  testLocalScope(void) {
    __block TestObject *__weak to __unused = [[TestObject alloc] init];
    // when we leave the scope a byref release call is made
    // this recovers the __block storage but leaves the contents alone
    // XXX make 10^6 of these to make sure we collect 'em
}

@end

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    testLocalScope();
    if (RetainCalled || ReleaseCalled) {
        fail("testLocalScope had some problems");
    }
        
    __block TestObject *__weak to = [[TestObject alloc] init];
    void (^block)(void) = ^ { printf("is it still real? %p\n", to); };
    (void)Block_copy(block);
    if (RetainCalled) {
        fail("Block_copy retain had some problems");
    }
    if (ReleaseCalled) {
        fail("Block_copy release had some problems");
    }
    [pool drain];

    succeed(__FILE__);
}
