/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  goto.c
 *  testObjects
 *
 *  Created by Blaine Garst on 10/17/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */
 
// TEST_CONFIG 
// rdar://6289031

#include <stdio.h>
#include "test.h"

int main()
{
    __block int val = 0;
    
    ^{ val = 1; }();
    
    if (val == 0) {
        goto out_bad; // error: local byref variable val is in the scope of this goto
    }
    
    succeed(__FILE__);

 out_bad:
    fail("val not updated!");
}
