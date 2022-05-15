/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  retainproperty.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// TEST_CFLAGS -framework Foundation


#include <stdio.h>
#include "test.h"

@interface TestObject {

}
@property(copy, readonly) int (^getInt)(void);
@end



int main() {
    succeed(__FILE__);
}
