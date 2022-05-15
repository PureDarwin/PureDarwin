/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import <Block.h>
#import <Block_private.h>
#import "test.h"

int recovered = 0;

@interface TestObject : NSObject {
}
@end
@implementation TestObject
- (void)dealloc {
    ++recovered;
    [super dealloc];
}
@end

typedef struct {
    struct Block_layout layout;  // assumes copy helper
    struct Block_byref *byref_ptr;
} Block_with_byref;

void testRoutine() {
    __block id to = [[TestObject alloc] init];
    void (^b)(void) = [^{ [to self]; } copy];
    for (int i = 0; i < 10; ++i)
        [b retain];
    for (int i = 0; i < 10; ++i)
        [b release];
    for (int i = 0; i < 10; ++i)
        (void)Block_copy(b);            // leak
    for (int i = 0; i < 10; ++i)
        Block_release(b);
    for (int i = 0; i < 10; ++i) {
        (void)Block_copy(b);   // make sure up
        Block_release(b);  // and down work under GC
    }
    [b release];
    [to release];
    // block_byref_release needed under non-GC to get rid of testobject
}
    

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    for (int i = 0; i < 200; ++i)   // do enough to trigger TLC if GC is on
        testRoutine();
    [pool drain];

    if (recovered == 0) {
        fail("didn't recover byref block variable");
    }

    succeed(__FILE__);
}
