/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

/*
 *  variadic.c
 *  testObjects
 *
 *  Created by Blaine Garst on 2/17/09.
 *  Copyright 2009 Apple. All rights reserved.
 *
 */

// PURPOSE Test that variadic arguments compile and work for Blocks
// TEST_CONFIG

#import <stdarg.h>
#import <stdio.h>
#import "test.h"

int main() {
    
    long (^addthem)(const char *, ...) = ^long (const char *format, ...){
        va_list argp;
        const char *p;
        int i;
        char c;
        double d;
        long result = 0;
        va_start(argp, format);
        //printf("starting...\n");
        for (p = format; *p; p++) switch (*p) {
            case 'i':
                i = va_arg(argp, int);
                //printf("i: %d\n", i);
                result += i;
                break;
            case 'd':
                d = va_arg(argp, double);
                //printf("d: %g\n", d);
                result += (int)d;
                break;
            case 'c':
                c = va_arg(argp, int);
                //printf("c: '%c'\n", c);
                result += c;
                break;
        }
        //printf("...done\n\n");
        return result;
    };
    long testresult = addthem("ii", 10, 20);
    if (testresult != 30) {
        fail("got wrong result: %ld", testresult);
    }
    testresult = addthem("idc", 30, 40.0, 'a');
    if (testresult != (70+'a')) {
        fail("got different wrong result: %ld", testresult);
    }

    succeed(__FILE__);
}


