/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG RUN=0


/*
TEST_BUILD_OUTPUT
.*k-and-r.c:26:11: error: incompatible block pointer types assigning to 'char \(\^\)\(\)' from 'char \(\^\)\(char\)'
OR
.*k-and-r.c:26:11: error: assigning to 'char \(\^\)\(\)' from incompatible type 'char \(\^\)\(char\)'
.*k-and-r.c:27:20: error: too many arguments to block call, expected 0, have 1
.*k-and-r.c:28:20: error: too many arguments to block call, expected 0, have 1
END
*/

#import <stdio.h>
#import <stdlib.h>
#import "test.h"

int main() {
    char (^rot13)();
    rot13 = ^(char c) { return (char)(((c - 'a' + 13) % 26) + 'a'); };
    char n = rot13('a');
    char c = rot13('p');
    if ( n != 'n' || c != 'c' ) {
        fail("rot13('a') returned %c, rot13('p') returns %c\n", n, c);
    }

    fail("should not compile");
}
