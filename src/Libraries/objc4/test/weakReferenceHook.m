/*
    TEST_CONFIG MEM=mrc
    TEST_ENV OBJC_DISABLE_NONPOINTER_ISA=YES
*/

#include "test.h"
#include "testroot.i"

bool hasWeakRefs = false;

@interface TestRoot (WeakHooks)
@end

@implementation TestRoot (WeakHooks)

- (void)_setWeaklyReferenced {
  hasWeakRefs = true;
}

// -_setWeaklyReferenced is currently limited to raw-isa custom-rr to avoid overhead
- (void) release {
}

@end

int main() {
    id obj = [TestRoot new];
    id wobj = nil;
    objc_storeWeak(&wobj, obj);
    testassert(hasWeakRefs == true);

    id out = objc_loadWeak(&wobj);
    testassert(out == obj);
  
    objc_storeWeak(&wobj, nil);
    out = objc_loadWeak(&wobj);
    testassert(out == nil);

    hasWeakRefs = false;
    objc_storeWeak(&wobj, obj);
    testassert(hasWeakRefs == true);


    out = objc_loadWeak(&wobj);
    testassert(out == obj);
    objc_storeWeak(&wobj, nil);
  
    succeed(__FILE__);
}
