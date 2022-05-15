/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import <Block_private.h>
#import "test.h"

typedef void (^void_block_t)(void);

int main () {
    void_block_t c = ^{ NSLog(@"Hello!"); };
    
    //printf("global block c looks like: %s\n", _Block_dump(c));
    int j;
    for (j = 0; j < 1000; j++)
    {
        void_block_t d = [c copy];
        //if (j == 0) printf("copy looks like %s\n", _Block_dump(d));
        [d release];
    }

    succeed(__FILE__);
}
