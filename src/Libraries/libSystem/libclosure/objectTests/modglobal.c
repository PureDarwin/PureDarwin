/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#include <stdio.h>
#include "test.h"

// TEST_CONFIG

int AGlobal;

int main() {
    void (^f)(void) __unused = ^ { AGlobal++; };
    
    succeed(__FILE__);
}
