//TEST_CONFIG MEM=mrc ARCH=x86_64,ARM64,ARM64e
//TEST_ENV OBJC_DISABLE_AUTORELEASE_COALESCING=NO OBJC_DISABLE_AUTORELEASE_COALESCING_LRU=NO

#include "test.h"
#import <Foundation/NSObject.h>
#include <os/feature_private.h>

@interface Counter: NSObject {
@public
    int retains;
    int releases;
    int autoreleases;
}
@end
@implementation Counter

- (id)retain {
    retains++;
    return [super retain];
}

- (oneway void)release {
    releases++;
    [super release];
}

- (id)autorelease {
    autoreleases++;
    return [super autorelease];
}

- (void)dealloc {
    testprintf("%p dealloc\n", self);
    [super dealloc];
}

@end

// Create a number of objects, autoreleasing each one a number of times in a
// round robin fashion. Verify that each object gets sent retain, release, and
// autorelease the correct number of times. Verify that the gap between
// autoreleasepool pointers is the given number of objects. Note: this will not
// work when the pool hits a page boundary, to be sure to stay under that limit.
void test(int objCount, int autoreleaseCount, int expectedGap) {
    testprintf("Testing %d objects, %d autoreleases, expecting gap of %d\n",
               objCount, autoreleaseCount, expectedGap);

    Counter *objs[objCount];
    for (int i = 0; i < objCount; i++)
        objs[i] = [Counter new];

    for (int j = 0; j < autoreleaseCount; j++)
        for (int i = 0; i < objCount; i++)
            [objs[i] retain];

    for (int i = 0; i < objCount; i++) {
        testassertequal(objs[i]->retains, autoreleaseCount);
        testassertequal(objs[i]->releases, 0);
        testassertequal(objs[i]->autoreleases, 0);
    }

    void *outer = objc_autoreleasePoolPush();
    uintptr_t outerAddr = (uintptr_t)outer;
    for (int j = 0; j < autoreleaseCount; j++)
        for (int i = 0; i < objCount; i++)
            [objs[i] autorelease];
    for (int i = 0; i < objCount; i++) {
        testassertequal(objs[i]->retains, autoreleaseCount);
        testassertequal(objs[i]->releases, 0);
        testassertequal(objs[i]->autoreleases, autoreleaseCount);
    }

    void *inner = objc_autoreleasePoolPush();
    uintptr_t innerAddr = (uintptr_t)inner;
    testprintf("outer=%p inner=%p\n", outer, inner);
    // Do one more autorelease in the inner pool to make sure we correctly
    // handle pool boundaries.
    for (int i = 0; i < objCount; i++)
        [[objs[i] retain] autorelease];
    for (int i = 0; i < objCount; i++) {
        testassertequal(objs[i]->retains, autoreleaseCount + 1);
        testassertequal(objs[i]->releases, 0);
        testassertequal(objs[i]->autoreleases, autoreleaseCount + 1);
    }

    objc_autoreleasePoolPop(inner);
    for (int i = 0; i < objCount; i++) {
        testassertequal(objs[i]->retains, autoreleaseCount + 1);
        testassertequal(objs[i]->releases, 1);
        testassertequal(objs[i]->autoreleases, autoreleaseCount + 1);
    }

    objc_autoreleasePoolPop(outer);
    for (int i = 0; i < objCount; i++) {
        testassertequal(objs[i]->retains, autoreleaseCount + 1);
        testassertequal(objs[i]->releases, autoreleaseCount + 1);
        testassertequal(objs[i]->autoreleases, autoreleaseCount + 1);
    }

    intptr_t gap = innerAddr - outerAddr;
    testprintf("gap=%ld\n", gap);
    testassertequal(gap, expectedGap * sizeof(id));

    // Destroy our test objects.
    for (int i = 0; i < objCount; i++)
        [objs[i] release];
}

int main()
{
    // Push a pool here so test() doesn't see a placeholder.
    objc_autoreleasePoolPush();

    test(1, 1, 2);
    test(1, 2, 2);
    test(1, 10, 2);
    test(1, 100, 2);
    test(1, 70000, 3);

    test(2, 1, 3);
    test(2, 2, 3);
    test(2, 10, 3);
    test(2, 100, 3);
    test(2, 70000, 5);

    test(3, 1, 4);
    test(3, 2, 4);
    test(3, 10, 4);
    test(3, 100, 4);
    test(3, 70000, 7);

    test(4, 1, 5);
    test(4, 2, 5);
    test(4, 10, 5);
    test(4, 100, 5);
    test(4, 70000, 9);

    test(5, 1, 6);
    test(5, 2, 11);

    succeed(__FILE__);
}
