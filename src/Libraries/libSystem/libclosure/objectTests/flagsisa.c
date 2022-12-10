/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG
// rdar://6310599

#include <stdio.h>
#include "test.h"

int main()
{
 	__block int flags;
 	__block void *isa;
 	
 	(void)^{ flags=1; isa = (void *)isa; };
        succeed(__FILE__);
}

