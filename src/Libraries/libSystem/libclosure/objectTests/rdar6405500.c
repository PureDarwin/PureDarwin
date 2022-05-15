/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG rdar://6405500

#import <stdio.h>
#import <stdlib.h>
#import <dispatch/dispatch.h>
#import "test.h"

int main () {
    __block void (^blockFu)(size_t t);
    blockFu = ^(size_t t){
        if (t == 20) {
            succeed(__FILE__);
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{ blockFu(20); });
        }
    };
    
    dispatch_apply(10, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), blockFu);
    dispatch_main();
    fail("unreachable");
}
