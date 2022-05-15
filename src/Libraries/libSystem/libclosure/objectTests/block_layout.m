/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*  block_layout.m
    Created by Patrick Beard on 3 Sep 2010
*/

// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import <Block.h>
#import <Block_private.h>
#import <dispatch/dispatch.h>
#import <assert.h>
#import "test.h"

int main (int argc, char const* argv[]) {
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    
    NSObject *o = [NSObject new];
    NSString *s = [NSString stringWithFormat:@"argc = %d, argv = %p", argc, argv];

    dispatch_block_t block = ^{
        NSLog(@"o = %@", o);
        NSLog(@"s = %@", s);
    };

        
    const char *layout = _Block_extended_layout(block);
    testprintf("layout %p\n", layout);
    assert (layout == (void*)0x200);

    const char *gclayout = _Block_layout(block);
    testprintf("GC layout %p\n", gclayout);
    assert (gclayout == NULL);

    block = [block copy];
    
    layout = _Block_extended_layout(block);
    testprintf("layout %p\n", layout);
    assert (layout == (void*)0x200);

    gclayout = _Block_layout(block);
    testprintf("GC layout %p\n", gclayout);
    assert (gclayout == NULL);
    
    block();
    [block release];
    
    [pool drain];
    
    succeed(__FILE__);
}
