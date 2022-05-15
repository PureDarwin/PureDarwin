/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//  -*- mode:C; c-basic-offset:4; tab-width:4; intent-tabs-mode:nil;  -*-
// TEST_CONFIG

#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <stdarg.h>
#import "test.h"

int main () {
    int (^sumn)(int n, ...) = ^(int n, ...){
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
    int six = sumn(3, 1, 2, 3);
    
    if ( six != 6 ) {
        fail("Expected 6 but got %d", six);
    }

    succeed(__FILE__);
}
