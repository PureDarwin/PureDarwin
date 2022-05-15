/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  weakblockrecover.m
//  testObjects
//
//  Created by Blaine Garst on 11/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CFLAGS -framework Foundation

// rdar://5847976



#import <Foundation/Foundation.h>
#import <Block.h>
#import "test.h"

int Allocated = 0;
int Recovered = 0;

@interface TestObject : NSObject
@end

@implementation TestObject

- (id)init {
    ++Allocated;
    return self;
}
- (void)dealloc {
    ++Recovered;
    [super dealloc];
}

@end

void testRecovery() {
    NSMutableArray *listOfBlocks = [NSMutableArray new];
    for (int i = 0; i < 1000; ++i) {
        __block TestObject *__weak to = [[TestObject alloc] init];
        void (^block)(void) = ^ { printf("is it still real? %p\n", to); };
        [listOfBlocks addObject:[block copy]];
        [to release];
    }

    [listOfBlocks self]; // by using it here we keep listOfBlocks alive across the GC
}

int main() {
    testRecovery();
    if ((Recovered + 10) < Allocated) {
        fail("Only %d weakly referenced test objects recovered, vs %d allocated\n", Recovered, Allocated);
    }

    succeed(__FILE__);
}
