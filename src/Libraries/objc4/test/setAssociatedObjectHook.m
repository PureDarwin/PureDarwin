/*
    TEST_CONFIG MEM=mrc
    TEST_ENV OBJC_DISABLE_NONPOINTER_ISA=YES
*/

#include "test.h"
#include "testroot.i"

bool hasAssociations = false;

@interface TestRoot (AssocHooks)
@end

@implementation TestRoot (AssocHooks)

- (void)_noteAssociatedObjects {
  hasAssociations = true;
}

// -_noteAssociatedObjects is currently limited to raw-isa custom-rr to avoid overhead
- (void) release {
}

@end

int main() {
    id obj = [TestRoot new];
    id value = [TestRoot new];
    const void *key = "key";
    objc_setAssociatedObject(obj, key, value, OBJC_ASSOCIATION_RETAIN);
    testassert(hasAssociations == true);

    id out = objc_getAssociatedObject(obj, key);
    testassert(out == value);

    hasAssociations = false;
    key = "key2";
    objc_setAssociatedObject(obj, key, value, OBJC_ASSOCIATION_RETAIN);
    testassert(hasAssociations == false); //only called once


    out = objc_getAssociatedObject(obj, key);
    testassert(out == value);

    succeed(__FILE__);
}
