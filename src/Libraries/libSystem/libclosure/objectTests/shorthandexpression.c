/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */
/*
 *  shorthandexpression.c
 *  testObjects
 *
 *  Created by Blaine Garst on 9/16/08.
 *  Copyright 2008 Apple. All rights reserved.
 *
 */

// TEST_CONFIG RUN=0

/*
TEST_BUILD_OUTPUT
.*shorthandexpression.c:27:\d+: error: expected expression
END
*/

#include <stdio.h>
#include "test.h"

void foo() {
    int (^b)(void) __unused = ^(void)printf("hello world\n");
}

int main() {
    fail("this shouldn't compile\n");
}
