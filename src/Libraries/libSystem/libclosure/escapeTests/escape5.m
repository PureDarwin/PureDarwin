/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  escape5.m
//  btest
//
//  Created by Apple on 6/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//


#import "common.h"



void willThrow() {
    @throw [NSException exceptionWithName:@"funny" reason:@"nothing" userInfo:nil];
}
void test(void) {
	BYREF int i = 0;
        @try {
            willThrow();
        }
        @catch(...) {
        }
        lastUse(i);
}


