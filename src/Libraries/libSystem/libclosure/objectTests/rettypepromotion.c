/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  rettypepromotion.c
 *  testObjects
 *
 *  Created by Blaine Garst on 11/3/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */
 
// TEST_CONFIG RUN=0
/*
TEST_BUILD_OUTPUT
.*rettypepromotion.c:44:19: error: incompatible block pointer types passing 'e \(\^\)\(void \*, void \*\)' to parameter of type 'uint64_t \(\^\)\(void \*, void \*\)'
.*rettypepromotion.c:39:31: note: passing argument to parameter 'comp' here
OR
.*rettypepromotion.c:44:5: error: no matching function for call to 'sortWithBlock'
.*rettypepromotion.c:39:6: note: candidate function not viable: no known conversion from 'e \(\^\)\(void \*, void \*\)' to 'uint64_t \(\^\)\(void \*, void \*\)' for 1st argument
END
 */



// these lines intentionally left blank



#include <stdio.h>
#include <stdlib.h>
#include "test.h"

typedef enum { LESS = -1, EQUAL, GREATER } e;

void sortWithBlock(uint64_t (^comp)(void *arg1, void *arg2)) {
    comp(0, 0);
}

int main() {
    sortWithBlock(^(void *arg1 __unused, void *arg2 __unused) {
        if (random()) return LESS;
        if (random()) return EQUAL;
        return GREATER;
    });

    succeed(__FILE__);
}
