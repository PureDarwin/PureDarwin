/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  notcopied.m
//  testObjects
//
//  Created by Blaine Garst on 2/12/09.
//  Copyright 2009 Apple. All rights reserved.
//

// TEST_CFLAGS -framework Foundation

// rdar://6557292
// Test that a __block Block variable with a reference to a stack based Block is not copied
// when a Block referencing the __block Block varible is copied.
// No magic for __block variables.

#import <stdio.h>
#import <Block.h>
#import <Block_private.h>
#import <Foundation/Foundation.h>
#import "test.h"

int Retained = 0;

@interface TestObject : NSObject
@end
@implementation TestObject
- (id)retain {
    Retained = 1;
    return [super retain];
}
@end


int main() {
    TestObject *to = [[TestObject alloc] init];
    __block void (^someBlock)(void) = ^ { [to self]; };
    void (^someOtherBlock)(void) = ^ {
          someBlock();   // reference someBlock.  It shouldn't be copied under the new rules.
    };
    someOtherBlock = [someOtherBlock copy];
    if (Retained != 0) {
        fail("__block Block was copied when it shouldn't have");
    }

    succeed(__FILE__);
}
