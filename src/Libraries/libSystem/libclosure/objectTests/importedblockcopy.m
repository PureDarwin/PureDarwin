/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  importedblockcopy.m
//  testObjects
//
//  Created by Blaine Garst on 10/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

// rdar://6297435
// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import "Block.h"
#import "test.h"

int Allocated = 0;
int Reclaimed = 0;

@interface TestObject : NSObject
@end

@implementation TestObject
- (void) dealloc {
    ++Reclaimed;
    [super dealloc];
}

- (id)init {
    self = [super init];
    ++Allocated;
    return self;
}

@end

void theTest() {
    // establish a block with an object reference
    TestObject *to = [[TestObject alloc] init];
    void (^inner)(void) = ^ {
        [to self];  // something that will hold onto "to"
    };
    // establish another block that imports the first one...
    void (^outer)(void) = ^ {
        inner();
        inner();
    };
    // now when we copy outer the compiler will _Block_copy_assign inner
    void (^outerCopy)(void) = Block_copy(outer);
    // but when released, at least under GC, it won't let go of inner (nor its import: "to")
    Block_release(outerCopy);
    [to release];
}


int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    for (int i = 0; i < 200; ++i)
        theTest();
    [pool drain];

    if ((Reclaimed+10) <= Allocated) {
        fail("whoops, reclaimed only %d of %d allocated", Reclaimed, Allocated);
    }

    succeed(__FILE__);
}
