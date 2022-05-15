/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  nullblockisa.m
//  testObjects
//
//  Created by Blaine Garst on 9/24/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CONFIG
// rdar://6244520

#include <stdio.h>
#include <stdlib.h>
#include <Block_private.h>
#include "test.h"

void check(void (^b)(void)) {
    struct _custom {
        struct Block_layout layout;
        struct Block_byref *innerp;
    } *custom  = (struct _custom *)(void *)(b);
    //printf("block is at %p, size is %lx, inner is %p\n", (void *)b, Block_size(b), innerp);
    if (custom->innerp->isa != (void *)NULL) {
        fail("not a NULL __block isa");
    }
    return;
}
        
int main() {

   __block int i;
   
   check(^{ printf("%d\n", ++i); });

   succeed(__FILE__);
}
   
