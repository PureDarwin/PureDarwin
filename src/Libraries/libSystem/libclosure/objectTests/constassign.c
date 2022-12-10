/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  constassign.c
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.

// TEST_CONFIG RUN=0

/*
TEST_BUILD_OUTPUT
.*constassign.c:38:12: error: cannot assign to variable 'blockA' with const-qualified type 'void \(\^const\)\((void)?\)'
.*constassign.c:37:18: note: .*
.*constassign.c:39:10: error: cannot assign to variable 'fptr' with const-qualified type 'void \(\*const\)\((void)?\)'
.*constassign.c:36:18: note: .*
END
*/



// shouldn't be able to assign to a const pointer
// CONFIG error: assignment of read-only

#import <stdio.h>
#import "test.h"

void foo(void) { printf("I'm in foo\n"); }
void bar(void) { printf("I'm in bar\n"); }

int main() {
    void (*const fptr)(void) = foo;
    void (^const  blockA)(void) = ^ { printf("hello\n"); };
    blockA = ^ { printf("world\n"); } ;
    fptr = bar;
    fail("should not compile");
}
