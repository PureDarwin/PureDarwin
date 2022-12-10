/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  localisglobal.c
 *  testObjects
 *
 *  Created by Blaine Garst on 9/29/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 */

// TEST_CONFIG
// rdar://6230297

#include <stdio.h>
#include "test.h"

void (^global)(void) = ^{ printf("hello world\n"); };

int aresame(void *first, void *second) {
    long *f = (long *)first;
    long *s = (long *)second;
    return *f == *s;
}
int main() {
    int i = 10;
    void (^local)(void) = ^ { printf("hi %d\n", i); };
    void (^localisglobal)(void) = ^ { printf("hi\n"); };
    
    if (aresame(local, localisglobal)) {
        fail("local block could be global, but isn't");
    }
    if (!aresame(global, localisglobal)) {
        fail("local block is not global, not stack, what is it??");
    }

    succeed(__FILE__);
}
