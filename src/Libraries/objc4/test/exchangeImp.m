/*
TEST_BUILD_OUTPUT
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
.*exchangeImp.m:\d+:\d+: warning: null passed to a callee that requires a non-null argument \[-Wnonnull\](\n.* note: expanded from macro 'checkExchange')?
END
*/

#include "test.h"
#include "testroot.i"
#include <objc/runtime.h>

static int state;
static int swizzleOld;
static int swizzleNew;
static int swizzleB;

#define ONE 1
#define TWO 2
#define LENGTH 3
#define COUNT 4

@interface Super : TestRoot @end
@implementation Super
+(void) one { state = ONE; }
+(void) two { state = TWO; }
+(void) length { state = LENGTH; }
+(void) count { state = COUNT; }

-(void) swizzleTarget {
    swizzleOld++;
}
-(void) swizzleReplacement {
    swizzleNew++;
}
@end

#define checkExchange(s1, v1, s2, v2)                                   \
    do {                                                                \
        Method m1, m2;                                                  \
                                                                        \
        testprintf("Check unexchanged version\n");                      \
        state = 0;                                                      \
        [Super s1];                                                     \
        testassert(state == v1);                                        \
        state = 0;                                                      \
        [Super s2];                                                     \
        testassert(state == v2);                                        \
                                                                        \
        testprintf("Exchange\n");                                       \
        m1 = class_getClassMethod([Super class], @selector(s1));        \
        m2 = class_getClassMethod([Super class], @selector(s2));        \
        testassert(m1);                                                 \
        testassert(m2);                                                 \
        method_exchangeImplementations(m1, m2);                         \
                                                                        \
        testprintf("Check exchanged version\n");                        \
        state = 0;                                                      \
        [Super s1];                                                     \
        testassert(state == v2);                                        \
        state = 0;                                                      \
        [Super s2];                                                     \
        testassert(state == v1);                                        \
                                                                        \
        testprintf("NULL should do nothing\n");                         \
        method_exchangeImplementations(m1, NULL);                       \
        method_exchangeImplementations(NULL, m2);                       \
        method_exchangeImplementations(NULL, NULL);                     \
                                                                        \
        testprintf("Make sure NULL did nothing\n");                     \
        state = 0;                                                      \
        [Super s1];                                                     \
        testassert(state == v2);                                        \
        state = 0;                                                      \
        [Super s2];                                                     \
        testassert(state == v1);                                        \
                                                                        \
        testprintf("Put them back\n");                                  \
        method_exchangeImplementations(m1, m2);                         \
                                                                        \
        testprintf("Check restored version\n");                         \
        state = 0;                                                      \
        [Super s1];                                                     \
        testassert(state == v1);                                        \
        state = 0;                                                      \
        [Super s2];                                                     \
        testassert(state == v2);                                        \
    } while (0) 

@interface A : Super
@end
@implementation A
@end

@interface B : Super
@end
@implementation B
- (void) swizzleTarget {
    swizzleB++;
}
@end

@interface C : Super
@end
@implementation C
- (void) hello { }
@end

static IMP findInCache(Class cls, SEL sel)
{
    struct objc_imp_cache_entry *ents;
    int count;
    IMP ret = nil;

    ents = class_copyImpCache(cls, &count);
    for (int i = 0; i < count; i++) {
        if (ents[i].sel == sel) {
            ret = ents[i].imp;
            break;
        }
    }
    free(ents);
    return ret;
}

int main()
{
    // Check ordinary selectors
    checkExchange(one, ONE, two, TWO);

    // Check vtable selectors
    checkExchange(length, LENGTH, count, COUNT);

    // Check ordinary<->vtable and vtable<->ordinary
    checkExchange(count, COUNT, one, ONE);
    checkExchange(two, TWO, length, LENGTH);

    Super *s = [Super new];
    A *a = [A new];
    B *b = [B new];
    C *c = [C new];

    // cache swizzleTarget in Super, A and B
    [s swizzleTarget];
    testassert(swizzleOld == 1);
    testassert(swizzleNew == 0);
    testassert(swizzleB == 0);
    testassert(findInCache([Super class], @selector(swizzleTarget)) != nil);

    [a swizzleTarget];
    testassert(swizzleOld == 2);
    testassert(swizzleNew == 0);
    testassert(swizzleB == 0);
    testassert(findInCache([A class], @selector(swizzleTarget)) != nil);

    [b swizzleTarget];
    testassert(swizzleOld == 2);
    testassert(swizzleNew == 0);
    testassert(swizzleB == 1);
    testassert(findInCache([B class], @selector(swizzleTarget)) != nil);

    // prime C's cache too
    [c hello];
    testassert(findInCache([C class], @selector(hello)) != nil);

    Method m1 = class_getInstanceMethod([Super class], @selector(swizzleTarget));
    Method m2 = class_getInstanceMethod([Super class], @selector(swizzleReplacement));
    method_exchangeImplementations(m1, m2);

    // this should invalidate Super, A, but:
    // - not B because it overrides - swizzleTarget and hence doesn't care
    // - not C because it neither called swizzleTarget nor swizzleReplacement
    testassert(findInCache([Super class], @selector(swizzleTarget)) == nil);
    testassert(findInCache([A class], @selector(swizzleTarget)) == nil);
    testassert(findInCache([B class], @selector(swizzleTarget)) != nil);
    testassert(findInCache([C class], @selector(hello)) != nil);

    // now check that all lookups do the right thing
    [s swizzleTarget];
    testassert(swizzleOld == 2);
    testassert(swizzleNew == 1);
    testassert(swizzleB == 1);

    [a swizzleTarget];
    testassert(swizzleOld == 2);
    testassert(swizzleNew == 2);
    testassert(swizzleB == 1);

    [b swizzleTarget];
    testassert(swizzleOld == 2);
    testassert(swizzleNew == 2);
    testassert(swizzleB == 2);

    [c swizzleTarget];
    testassert(swizzleOld == 2);
    testassert(swizzleNew == 3);
    testassert(swizzleB == 2);

    succeed(__FILE__);
}
