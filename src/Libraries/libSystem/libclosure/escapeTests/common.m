/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#import "common.h"

int useCounter = 0;
void lastUse(int param) {
	++useCounter;
}

int bcounter = 0;
#if FARIBORZ
void _Block_byref_release(void *byrefblock) {
	++bcounter;
}
#endif

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    test();
    if (bcounter != useCounter) {
        printf("%s: byref block not released %d times: %d\n", argv[0], useCounter, bcounter);
        return 1;
    }
    printf("%s: ok\n", argv[0]);
    [pool drain];
    return 0;
}
