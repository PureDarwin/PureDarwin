/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  copyproperty.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import <stdio.h>
#import "test.h"

@interface TestObject : NSObject {
    long (^getInt)(void);
}
@property(copy) long (^getInt)(void);
@end

@implementation TestObject
@synthesize getInt;
@end

@interface CountingObject : NSObject
@end

int Retained = 0;

@implementation CountingObject
- (id) retain {
    Retained = 1;
    return [super retain];
}
@end

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    TestObject *to = [[TestObject alloc] init];
    CountingObject *co = [[CountingObject alloc] init];
    long (^localBlock)(void) = ^{ return 10L + (long)[co self]; };
    to.getInt = localBlock;    
    if (localBlock == to.getInt) {
        fail("block property not copied!!");
    }
    if (Retained == 0) {
        fail("didn't copy block import");
    }

    [pool drain];

    succeed(__FILE__);
}
