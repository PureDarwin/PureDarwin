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
	int counter = 0;
	while (counter < 10) {
		BYREF int i = 0;
		vv block = ^{  ++i; };
		if (counter > 5) {
			lastUse(i);
			break;
		}
		++counter;
		lastUse(i);
	}
}
