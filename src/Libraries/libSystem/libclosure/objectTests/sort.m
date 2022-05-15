/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

//
//  sort.m
//  testObjects
//
//  Created by Blaine Garst on 1/9/09.
//  Copyright 2009 Apple. All rights reserved.
//

// TEST_CFLAGS -framework Foundation

#import <Foundation/Foundation.h>
#import "test.h"

int main() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSArray *array __unused = [[NSArray array] sortedArrayUsingComparator:^(id one, id two __unused) { if ([one self]) return (NSComparisonResult)NSOrderedSame; return (NSComparisonResult)NSOrderedAscending; }];
    [pool drain];

    succeed(__FILE__);
}
