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
    unsigned long ps[30];
    int qs[30];
} BobTheStruct;

int main () {
    __block BobTheStruct fiddly;
    BobTheStruct copy;

    void (^incrementFiddly)() = ^{
        int i;
        for(i=0; i<30; i++) {
            fiddly.ps[i]++;
            fiddly.qs[i]++;
        }
    };
    
    memset(&fiddly, 0xA5, sizeof(fiddly));
    memset(&copy, 0x2A, sizeof(copy));    
    
    int i;
    for(i=0; i<30; i++) {
        fiddly.ps[i] = i * i * i;
        fiddly.qs[i] = -i * i * i;
    }
    
    copy = fiddly;
    incrementFiddly();

    if ( &copy == &fiddly ) {
        fail("struct wasn't copied");
    }
    for(i=0; i<30; i++) {
        //printf("[%d]: fiddly.ps: %lu, copy.ps: %lu, fiddly.qs: %d, copy.qs: %d\n", i, fiddly.ps[i], copy.ps[i], fiddly.qs[i], copy.qs[i]);
        if ( (fiddly.ps[i] != copy.ps[i] + 1) || (fiddly.qs[i] != copy.qs[i] + 1) ) {
            fail("struct contents were not incremented");
        }
    }
    
    succeed(__FILE__);
}
