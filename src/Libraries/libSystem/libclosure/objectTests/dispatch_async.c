/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG
// rdar://problem/6371811

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#include <Block.h>
#include "test.h"

void EnqueueStuff(dispatch_queue_t q)
{
    __block CFIndex counter;
    
    // above call has a side effect: it works around:
    // <rdar://problem/6225809> __block variables not implicitly imported into intermediate scopes
    dispatch_async(q, ^{
        counter = 0;
    });
    
    
    dispatch_async(q, ^{
        //printf("outer block.\n");
        counter++;
        dispatch_async(q, ^{
            //printf("inner block.\n");
            counter--;
            if(counter == 0) {
                succeed(__FILE__);
            }
        });
        if(counter == 0) {
            fail("already done? inconceivable!");
        }
    });        
}

int main () {
    dispatch_queue_t q = dispatch_queue_create("queue", NULL);

    EnqueueStuff(q);
    
    dispatch_main();
    fail("unreachable");
}
