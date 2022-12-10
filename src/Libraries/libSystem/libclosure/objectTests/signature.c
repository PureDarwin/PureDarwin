/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// PURPOSE check _Block_has_signature, _Block_signature, and _Block_use_stret.
// TEST_CONFIG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Block.h>
#include <Block_private.h>
#include "test.h"

#if __arm64__
#   define HAVE_STRET 0
#else
#   define HAVE_STRET 1
#endif

typedef struct bigbig {
    int array[512];
} BigStruct_t;

void* (^global)(void) = ^{ return malloc(sizeof(struct bigbig)); };
BigStruct_t (^global_stret)(void) = ^{ return *(BigStruct_t *)malloc(sizeof(struct bigbig)); };

int main()
{
    void* (^local)(void) = ^{ return malloc(sizeof(struct bigbig)); };
    BigStruct_t (^local_stret)(void) = ^{ return *(BigStruct_t *)malloc(sizeof(struct bigbig)); };

    testassert(_Block_has_signature(local));
    testassert(_Block_has_signature(global));
    testassert(_Block_has_signature(local_stret));
    testassert(_Block_has_signature(global_stret));
#  if __LP64__
#   define P "8"
#  else
#   define P "4"
#  endif
    testassert(0 == strcmp(_Block_signature(local), "^v"P"@?0"));
    testassert(0 == strcmp(_Block_signature(global), "^v"P"@?0"));
    testassert(0 == strcmp(_Block_signature(local_stret), "{bigbig=[512i]}"P"@?0"));
    testassert(0 == strcmp(_Block_signature(global_stret), "{bigbig=[512i]}"P"@?0"));

    testassert(! _Block_use_stret(local));
    testassert(! _Block_use_stret(global));
#if HAVE_STRET
    testassert(_Block_use_stret(local_stret));
    testassert(_Block_use_stret(global_stret));
#else
    testassert(! _Block_use_stret(local_stret));
    testassert(! _Block_use_stret(global_stret));
#endif

    succeed(__FILE__);
}
