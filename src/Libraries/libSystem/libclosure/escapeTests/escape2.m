/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  escape2.m
//  btest
//
//  Created by Apple on 6/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//


#import "common.h"


void test(void) {
	// validate that escaping a context is enough
	if (getpid() % 2) {
		BYREF int i = 0;
		vv block = ^{ ++i; };
		vv blockCopy = Block_copy(block);
		lastUse(i);
		Block_release(blockCopy);
		lastUse(i);
	}
	else {
		BYREF int j = 0;
		vv block = ^{ j += 2; };
		vv blockCopy = Block_copy(block);
		lastUse(j);
		Block_release(blockCopy);
		lastUse(j);
	}
}
