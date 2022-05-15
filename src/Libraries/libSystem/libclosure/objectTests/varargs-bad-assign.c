/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

// TEST_CONFIG RUN=0

/*
TEST_BUILD_OUTPUT
.*varargs-bad-assign.c: In function 'main':
.*varargs-bad-assign.c:43: error: incompatible block pointer types assigning 'int \(\^\)\(int,  int,  int\)', expected 'int \(\^\)\(int\)'
OR
.*varargs-bad-assign.c: In function '.*main.*':
.*varargs-bad-assign.c:43: error: cannot convert 'int \(\^\)\(int, int, int, \.\.\.\)' to 'int \(\^\)\(int, \.\.\.\)' in assignment
OR
.*varargs-bad-assign.c:31:10: error:( incompatible block pointer types)? assigning to 'int \(\^\)\(int, \.\.\.\)' from( incompatible type)? 'int \(\^\)\(int, int, int, \.\.\.\)'
END
*/

#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <stdarg.h>
#import "test.h"

int main () {
    int (^sumn)(int n, ...);
    int six = 0;
    
    sumn = ^(int a __unused, int b __unused, int n, ...){
        int result = 0;
        va_list numbers;
        int i;

        va_start(numbers, n);
        for (i = 0 ; i < n ; i++) {
            result += va_arg(numbers, int);
        }
        va_end(numbers);

        return result;
    };

    six = sumn(3, 1, 2, 3);

    if ( six != 6 ) {
        fail("Expected 6 but got %d", six);
    }
    
    succeed(__FILE__);
}
