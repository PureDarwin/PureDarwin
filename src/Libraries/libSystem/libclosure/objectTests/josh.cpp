/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG CC=clang

#include <stdio.h>
#include <stdlib.h>
#include "test.h"

struct MyStruct {
    int something;
};

struct TestObject {

        void test(void){
            {
                MyStruct first __unused;   // works
            }
            void (^b)(void) __unused = ^{ 
                MyStruct inner __unused;  // fails to compile!
            };
        }
};

    

int main() {
    succeed(__FILE__);
}
