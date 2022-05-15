/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  sizeof.c
 *  testObjects
 *
 *  Created by Blaine Garst on 2/17/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

// TEST_CONFIG RUN=0

/*
TEST_BUILD_OUTPUT
.*sizeof.c: In function 'main':
.*sizeof.c:36: error: invalid type argument of 'unary \*'
OR
.*sizeof.c: In function '.*main.*':
.*sizeof.c:36: error: invalid application of 'sizeof' to a function type
OR
.*sizeof.c:36:(47|51): error: indirection requires pointer operand \('void \(\^\)\((void)?\)' invalid\)
END
 */

#include <stdio.h>
#include "test.h"

int main() {
    void (^aBlock)(void) = ^{ printf("hellow world\n"); };

    fail("the size of a block is %ld", sizeof(*aBlock));
}
