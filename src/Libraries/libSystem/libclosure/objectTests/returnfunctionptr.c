/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// rdar://6339747 but wasn't
// TEST_CONFIG

#include <stdio.h>
#include "test.h"

int (*funcptr)(long);

int (*(^b)(char))(long);

int main()  {
    // implicit is fine
    b = ^(char x __unused) { return funcptr; };
    // explicit never parses
    b = ^int (*(char x __unused))(long) { return funcptr; };

    succeed(__FILE__);
}
