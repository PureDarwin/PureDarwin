/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  retainrelease.m
//  bocktest
//
//  Created by Blaine Garst on 3/21/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import "test.h"

@interface TestObject : NSObject {
}
@end

int GlobalInt = 0;

@implementation TestObject
- (id) retain {
    ++GlobalInt;
    return self;
}


@end

int main(int argc, char *argv[] __unused) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   // an object should not be retained within a stack Block
   TestObject *to = [[TestObject alloc] init];
   TestObject *to2 = [[TestObject alloc] init];
   void (^blockA)(void) __unused = ^ { [to self]; printf("using argc %d\n", argc); [to2 self]; };
   if (GlobalInt != 0) {
       fail("object retained inside stack closure");
   }
   [pool drain];

   succeed(__FILE__);
}
   
