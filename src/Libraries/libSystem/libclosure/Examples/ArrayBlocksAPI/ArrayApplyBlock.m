/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#import <Foundation/Foundation.h>

#ifndef __BLOCKS__
#error compiler does not support blocks.
#endif

#if !NS_BLOCKS_AVAILABLE
#error Blocks don't appear to be available, according to the Foundation.
#endif

int main (int argc, const char * argv[]) {
    NSArray *array = [NSArray arrayWithObjects: @"A", @"B", @"C", @"A", @"B", @"Z",@"G", @"are", @"Q", nil];
	NSSet *filterSet = [NSSet setWithObjects: @"A", @"Z", @"Q", nil];
    
    [array enumerateObjectsUsingBlock:  ^(id anObject, NSUInteger idx, BOOL *stop) {
        NSLog(@"%d: \t %@", idx, anObject);
        if (idx == 4) {
            NSLog(@"\tStopping Enumeration.");
            *stop = YES;
        }
    }];
    
    NSIndexSet *indexSet = [array indexesOfObjectsPassingTest: ^(id anObject, NSUInteger idx, BOOL *stop) {
        return [filterSet containsObject: anObject];
    }];
    NSLog(@"Filtered: %@", [array objectsAtIndexes: indexSet]);
    
	NSLog(@"Case Insensitive Sorted: %@", [array sortedArrayUsingComparator: ^(id a, id b) { return [a caseInsensitiveCompare: b]; }]);
    
    return 0;
}
