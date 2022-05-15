/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//  -*- mode:C; c-basic-offset:4; tab-width:4; intent-tabs-mode:nil;  -*-
// TEST_CONFIG

#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import "test.h"

typedef struct {
  int a;
  int b;
} MiniStruct;

int main () {
    MiniStruct inny;
    MiniStruct outty;
    MiniStruct (^copyStruct)(MiniStruct);
    
    memset(&inny, 0xA5, sizeof(inny));
    memset(&outty, 0x2A, sizeof(outty));    
    
    inny.a = 12;
    inny.b = 42;

    copyStruct = ^(MiniStruct aTinyStruct){ return aTinyStruct; };  // pass-by-value intrinsically copies the argument
    
    outty = copyStruct(inny);

    if ( &inny == &outty ) {
        fail("struct wasn't copied");
    }
    if ( (inny.a != outty.a) || (inny.b != outty.b) ) {
        fail("struct contents did not match");
    }
    
    succeed(__FILE__);
}
