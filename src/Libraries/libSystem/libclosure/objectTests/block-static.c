/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG

#include <stdio.h>
#include "test.h"

int main() {
    static int numberOfSquesals = 5;
    
    ^{ numberOfSquesals = 6; }();
    
    if (numberOfSquesals != 6) {
        fail("did not update static local, rdar://6177162");
    }

    succeed(__FILE__);
}

