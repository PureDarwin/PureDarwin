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

NSInteger sortStuff(id a, id  b, void *inReverse) {
    int reverse = (int) inReverse; // oops
    int result = [(NSString *)a compare: b];
    return reverse ? -result : result;
}

int main (int argc, const char * argv[]) {
    NSArray *stuff = [NSArray arrayWithObjects: @"SQUARED OFF", @"EIGHT CORNERS", @"90-DEGREE ANGLES", @"FLAT TOP", @"STARES STRAIGHT AHEAD", @"STOCK PARTS", nil];
    int inReverse = 1;
    
    NSLog(@"reverse func: %@", [stuff sortedArrayUsingFunction:sortStuff context: &inReverse]);
    NSLog(@"reverse block: %@", [stuff sortedArrayUsingComparator: ^(id a,  id b) {
        int result = [a compare: b];
        return inReverse ? -result : result;
    }]);

    inReverse = 0;

    NSLog(@"forward func: %@", [stuff sortedArrayUsingFunction:sortStuff context: &inReverse]);
    NSLog(@"forward block: %@", [stuff sortedArrayUsingComparator: ^(id a,  id b) {
        int result = [a compare: b];
        return inReverse ? -result : result;
    }]);
    
    return 0;
}
