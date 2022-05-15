/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  objectassign.c
 *  testObjects
 *
 *  Created by Blaine Garst on 10/28/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 */

// TEST_CONFIG
// rdar://6175959
// This just tests that the compiler is issuing the proper helper routines

#include <stdio.h>
#include "Block_private.h"
#include "test.h"

int AssignCalled = 0;
int DisposeCalled = 0;

// local copy instead of libSystem.B.dylib copy
void _Block_object_assign(void *destAddr __unused, const void *object __unused, const int isWeak __unused) {
    //printf("_Block_object_assign(%p, %p, %d) called\n", destAddr, object, isWeak);
    AssignCalled = 1;
}

void _Block_object_dispose(const void *object __unused, const int isWeak __unused) {
    //printf("_Block_object_dispose(%p, %d) called\n", object, isWeak);
    DisposeCalled = 1;
}

struct MyStruct {
    long isa;
    long field;
};

typedef struct MyStruct *__attribute__((NSObject)) MyStruct_t;

int main() {
    // create a block
    struct MyStruct X;
    MyStruct_t xp = (MyStruct_t)&X;
    xp->field = 10;
    void (^myBlock)(void) = ^{ printf("field is %ld\n", xp->field); };
    // should be a copy helper generated with a calls to above routines
    // Lets find out!
    struct Block_layout *bl = (struct Block_layout *)(void *)myBlock;
    if ((bl->flags & BLOCK_HAS_COPY_DISPOSE) != BLOCK_HAS_COPY_DISPOSE) {
        fail("no copy dispose!!!!");
    }
    // call helper routines directly.  These will, in turn, we hope, call the stubs above
    long destBuffer[256];
    //printf("destbuffer is at %p, block at %p\n", destBuffer, (void *)bl);
    //printf("dump is %s\n", _Block_dump(myBlock));

    _Block_get_copy_function(bl)(destBuffer, bl);
    _Block_get_dispose_function(bl)(bl);

    if (AssignCalled == 0) {
        fail("did not call assign helper!");
    }
    if (DisposeCalled == 0) {
        fail("did not call dispose helper");
    }

    succeed(__FILE__);
}
