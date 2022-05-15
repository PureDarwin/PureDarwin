/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  byrefcopyint.c
 *  testObjects
 *
 *  Created by Blaine Garst on 12/1/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

//
//  byrefcopyid.m
//  testObjects
//
//  Created by Blaine Garst on 5/13/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CONFIG

// Tests copying of blocks with byref ints
// rdar://6414583

#include <stdio.h>
#include <string.h>
#include <Block.h>
#include <Block_private.h>
#include "test.h"

typedef void (^voidVoid)(void);

voidVoid dummy;

void callVoidVoid(voidVoid closure) {
    closure();
}


voidVoid testRoutine(const char *whoami) {
    __block size_t dumbo = strlen(whoami);
    dummy = ^{
        //printf("incring dumbo from %d\n", dumbo);
        ++dumbo;
    };
    
    
    voidVoid copy = Block_copy(dummy);
    

    return copy;
}

int main(int argc __unused, char *argv[]) {
    voidVoid array[100];
    for (int i = 0; i <  100; ++i) {
        array[i] = testRoutine(argv[0]);
        array[i]();
    }
    for (int i = 0; i <  100; ++i) {
        Block_release(array[i]);
    }
    
    succeed(__FILE__);
}
