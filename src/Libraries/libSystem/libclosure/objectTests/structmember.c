/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  structmember.c
 *  testObjects
 *
 *  Created by Blaine Garst on 9/30/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 */

// TEST_CONFIG

#include <Block.h>
#include <Block_private.h>
#include <stdio.h>
#include "test.h"

int main() {
    struct stuff {
        long int a;
        long int b;
        long int c;
    } localStuff = { 10, 20, 30 };
    int d = 0;
    
    void (^a)(void) = ^ { printf("d is %d", d); };
    void (^b)(void) = ^ { printf("d is %d, localStuff.a is %lu", d, localStuff.a); };

    unsigned long nominalsize = Block_size(b) - Block_size(a);
#if __cplusplus__
    // need copy+dispose helper for C++ structures
    nominalsize += 2*sizeof(void*);
#endif
    if ((Block_size(b) - Block_size(a)) != nominalsize) {
        // testwarn("dump of b is %s", _Block_dump(b));
        fail("sizeof a is %lu, sizeof b is %lu, expected %lu", Block_size(a), Block_size(b), nominalsize);
    }

    succeed(__FILE__);
}


