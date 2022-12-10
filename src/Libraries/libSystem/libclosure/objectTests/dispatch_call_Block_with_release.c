/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#include <stdio.h>
#include <Block.h>
#include "test.h"

// TEST_CONFIG

void callsomething(const char *format __unused, int argument __unused) {
    asm("");
}

void
dispatch_call_Block_with_release2(void *block)
{
        void (^b)(void) = (void (^)(void))block;
        b();
        Block_release(b);
}

int main(int argc, char *argv[] __unused) {
     void (^b1)(void) = ^{ callsomething("argc is %d\n", argc); };
     void (^b2)(void) = ^{ callsomething("hellow world\n", 0); }; // global block now

     dispatch_call_Block_with_release2(Block_copy(b1));
     dispatch_call_Block_with_release2(Block_copy(b2));

     succeed(__FILE__);
}
