/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  nestedBlock.m
//  testObjects
//
//  Created by Blaine Garst on 6/24/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// test -retain

// TEST_CONFIG
// TEST_CFLAGS -framework Foundation

#import <stdio.h>
#import <Block.h>
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

void callVoidVoid(void (^closure)(void)) {
    closure();
}

int main(int argc, char *argv[] __unused) {
    TestObject *to = [[TestObject alloc] init];
    int i = argc;
    
    // use a copy & see that it updates i
    callVoidVoid(Block_copy(^{
        if (i > 0) {
            callVoidVoid(^{ [to self]; });
        }
    }));
    
    if (Retained == 0) {
        fail("didn't update Retained");
    }

    succeed(__FILE__);
}
