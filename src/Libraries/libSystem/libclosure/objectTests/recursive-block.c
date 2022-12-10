/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG

#include <stdio.h>
#include <Block.h>
#include <Block_private.h>
#include <stdlib.h>
#include "test.h"

int cumulation = 0;

int doSomething(int i) {
    cumulation += i;
    return cumulation;
}

void dirtyStack() {
    int i = (int)random();
    int j = doSomething(i);
    int k = doSomething(j);
    doSomething(i + j + k);
}

typedef void (^voidVoid)(void);

voidVoid testFunction() {
    int i = (int)random();
    __block voidVoid inner = ^{ doSomething(i); };
    //printf("inner, on stack, is %p\n", (void*)inner);
    /*__block*/ voidVoid outer = ^{
        //printf("will call inner block %p\n", (void *)inner);
        inner();
    };
    //printf("outer looks like: %s\n", _Block_dump(outer));
    voidVoid result = Block_copy(outer);
    //Block_release(inner);
    return result;
}


int main() {
    voidVoid block = testFunction();
    dirtyStack();
    block();
    Block_release(block);

    succeed(__FILE__);
}
