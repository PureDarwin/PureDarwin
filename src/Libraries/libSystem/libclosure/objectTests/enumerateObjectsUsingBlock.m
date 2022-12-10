/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#import <Foundation/Foundation.h>
#import "test.h"

// TEST_CFLAGS -framework Foundation

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSArray *empty = [NSArray new];
    [empty enumerateObjectsUsingBlock: ^(id obj __unused, NSUInteger idx __unused, BOOL *stop __unused) {
        fail("Block called when enumerating empty array");
    }];
    
    __block int callCount = 0;
    NSArray *three = [NSArray arrayWithObjects:
                      @"One",
                      @"Two",
                      @"Three",
                      nil];
    [three enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx __unused, BOOL *stop) {
        callCount++;
        if ([@"Two" isEqual: obj]) {
            *stop = YES;
        } else if ([@"Three" isEqual: obj]) {
            fail("Block called after stop was set");
        }
    }];
    if (callCount != 2) {
        fail("Block should have been called twice, actually counted %d", callCount);
    }
    
    [pool drain];

    succeed(__FILE__);
}
