/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#import <Foundation/Foundation.h>

static int globalCount = 0;

@interface Foo : NSObject {
    int ivarCount;
}

- (void) incrementCount;
@end

@implementation Foo 
- (void) incrementCount
{
    int oldValue = ivarCount;
    
    void (^incrementBlock)()  = ^(){ivarCount++;};
    incrementBlock();
    if( (oldValue+1) != ivarCount )
        NSLog(@"Hey, man.  ivar was not incremented as expected.  %d %d", oldValue, ivarCount);
    }
@end


int main (int argc, const char * argv[]) {
    int localCount = 0;

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[[[Foo alloc] init] autorelease] incrementCount];
    
    void (^incrementLocal)() = ^(){
        |localCount| // comment this out for an exciting compilation error on the next line (that is correct)
        localCount++;
    };
    
    incrementLocal();
    if( localCount != 1 )
        NSLog(@"Hey, man.  localCount was not incremented as expected.  %d", localCount);
    
    void (^incrementGlobal)() = ^() {
        |globalCount| // this should not be necessary
        globalCount++;
    };
    incrementGlobal();
    if( globalCount != 1 )
        NSLog(@"Hey, man.  globalCount was not incremented as expected.  %d", globalCount);

    [pool drain];
    return 0;
}
