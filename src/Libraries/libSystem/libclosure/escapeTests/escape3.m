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
	for (int counter = 0; counter < 10; ++counter) {
		BYREF int i = 0;
		vv block = ^{  ++i; };
		if (i < 9) {
			lastUse(i);
			continue; // leave scope early
		}
		lastUse(i);
	}
}
