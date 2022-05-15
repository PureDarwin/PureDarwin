/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CFLAGS -framework Foundation
//
// rdar://8295106
// use block variable in for..in statement

#import <Foundation/Foundation.h>
#import "test.h"

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSMutableArray *array = [NSMutableArray array];
    for (int i = 0; i < 200; ++i) {
        [array addObject:[[^{ return i; } copy] autorelease]];
    }
    int i = 0;
    for (int (^b)(void) in array) {
        testassert(b() == i++);
    }
    [pool drain];

    succeed(__FILE__);
}
