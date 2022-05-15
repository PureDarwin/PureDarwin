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
void innocent() {
    __block int i;
    @try {
        lastUse(i);
        willThrow();
    }
    @finally {
    }
}

void test(void) {
	@try {
            innocent();
        }
        @catch(...) {
        }
}


