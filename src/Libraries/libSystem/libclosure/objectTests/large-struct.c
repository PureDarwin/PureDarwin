/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG

#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import "test.h"

typedef struct {
    unsigned long ps[30];
    int qs[30];
} BobTheStruct;

int main () {
    BobTheStruct inny;
    BobTheStruct outty;
    BobTheStruct (^copyStruct)(BobTheStruct);
    int i;
    
    memset(&inny, 0xA5, sizeof(inny));
    memset(&outty, 0x2A, sizeof(outty));    
    
    for(i=0; i<30; i++) {
        inny.ps[i] = i * i * i;
        inny.qs[i] = -i * i * i;
    }
    
    copyStruct = ^(BobTheStruct aBigStruct){ return aBigStruct; };  // pass-by-value intrinsically copies the argument
    
    outty = copyStruct(inny);

    if ( &inny == &outty ) {
        fail("struct wasn't copied");
    }
    for(i=0; i<30; i++) {
        if ( (inny.ps[i] != outty.ps[i]) || (inny.qs[i] != outty.qs[i]) ) {
            fail("struct contents did not match.");
        }
    }
    
    succeed(__FILE__);
}
