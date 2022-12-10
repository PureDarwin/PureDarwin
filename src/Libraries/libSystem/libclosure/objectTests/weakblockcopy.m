/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  weakblock.m
//  testObjects
//
//  Created by Blaine Garst on 10/30/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// TEST_CFLAGS -framework Foundation
//
// Super basic test - does compiler a) compile and b) call out on assignments

#import <Foundation/Foundation.h>
#import "Block_private.h"
#import <pthread.h>
#import "test.h"

// provide our own version for testing

int GotCalled = 0;

int Errors = 0;

int recovered = 0;

@interface TestObject : NSObject {
}
@end

@implementation TestObject
- (id)retain {
    fail("Whoops, retain called!");
}
- (void)dealloc {
    ++recovered;
    [super dealloc];
}
@end


id (^testCopy(void))(void) {
    // create test object
    TestObject *to = [[TestObject alloc] init];
    __block TestObject *__weak  testObject = to;    // iniitialization does NOT require support function
    //id (^b)(void) = [^{ return testObject; } copy];  // g++ rejects this
    id (^b)(void) = [^id{ return testObject; } copy];
    return b;
}

void *test(void *arg __unused)
{
    NSMutableArray *array = (NSMutableArray *)arg;

    GotCalled = 0;
    for (int i = 0; i < 200; ++i) {
        [array addObject:testCopy()];
    }

    return NULL;
}

int main() {

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSMutableArray *array = [NSMutableArray array];

    pthread_t th;
    pthread_create(&th, NULL, test, array);
    pthread_join(th, NULL);

    for (id (^b)(void) in array) {
        if (b() == nil) {
            fail("whoops, lost a __weak __block id");
        }
    }
#if __has_feature(objc_arc)
#error fixme port this post-deallocation check from GC
    for (id (^b)(void) in array) {
            if (b() != nil) {
                fail("whoops, kept a __weak __block id");
            }
        }
    }
#endif

    [pool drain];

    succeed(__FILE__);
}
