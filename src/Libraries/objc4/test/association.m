// TEST_CONFIG

#include "test.h"
#include <Foundation/NSObject.h>
#include <objc/runtime.h>
#include <objc/objc-internal.h>
#include <Block.h>

static int values;
static int supers;
static int subs;

static const char *key = "key";


@interface Value : NSObject @end
@interface Super : NSObject @end
@interface Sub : NSObject @end

@interface Super2 : NSObject @end
@interface Sub2 : NSObject @end

@implementation Super 
-(id) init
{
    // rdar://8270243 don't lose associations after isa swizzling

    id value = [Value new];
    objc_setAssociatedObject(self, &key, value, OBJC_ASSOCIATION_RETAIN);
    RELEASE_VAR(value);

    object_setClass(self, [Sub class]);
    
    return self;
}

-(void) dealloc 
{
    supers++;
    SUPER_DEALLOC();
}

@end

@implementation Sub
-(void) dealloc 
{
    subs++;
    SUPER_DEALLOC();
}
@end

@implementation Super2
-(id) init
{
    // rdar://9617109 don't lose associations after isa swizzling

    id value = [Value new];
    object_setClass(self, [Sub2 class]);
    objc_setAssociatedObject(self, &key, value, OBJC_ASSOCIATION_RETAIN);
    RELEASE_VAR(value);
    object_setClass(self, [Super2 class]);
    
    return self;
}

-(void) dealloc 
{
    supers++;
    SUPER_DEALLOC();
}

@end

@implementation Sub2
-(void) dealloc 
{
    subs++;
    SUPER_DEALLOC();
}
@end

@implementation Value
-(void) dealloc {
    values++;
    SUPER_DEALLOC();
}
@end

@interface Sub59318867: NSObject @end
@implementation Sub59318867
+ (void)initialize {
  objc_setAssociatedObject(self, &key, self, OBJC_ASSOCIATION_ASSIGN);
}
@end

@interface CallOnDealloc: NSObject @end
@implementation CallOnDealloc {
    void (^_block)(void);
}
- (id)initWithBlock: (void (^)(void))block {
    _block = (__bridge id)Block_copy((__bridge void *)block);
    return self;
}
- (void)dealloc {
    _block();
    _Block_release((__bridge void *)_block);
    SUPER_DEALLOC();
}
@end

void TestReleaseLater(void) {
    int otherObjsCount = 100;
    char keys1[otherObjsCount];
    char keys2[otherObjsCount];
    char laterKey;

    __block int normalDeallocs = 0;
    __block int laterDeallocs = 0;

    {
        id target = [NSObject new];
        for (int i = 0; i < otherObjsCount; i++) {
            id value = [[CallOnDealloc alloc] initWithBlock: ^{ normalDeallocs++; }];
            objc_setAssociatedObject(target, keys1 + i, value, OBJC_ASSOCIATION_RETAIN);
            RELEASE_VALUE(value);
        }
        {
            id laterValue = [[CallOnDealloc alloc] initWithBlock: ^{
                testassertequal(laterDeallocs, 0);
                testassertequal(normalDeallocs, otherObjsCount * 2);
                laterDeallocs++;
            }];
            objc_setAssociatedObject(target, &laterKey, laterValue, (objc_AssociationPolicy)(OBJC_ASSOCIATION_RETAIN | _OBJC_ASSOCIATION_SYSTEM_OBJECT));
            RELEASE_VALUE(laterValue);
        }
        for (int i = 0; i < otherObjsCount; i++) {
            id value = [[CallOnDealloc alloc] initWithBlock: ^{ normalDeallocs++; }];
            objc_setAssociatedObject(target, keys2 + i, value, OBJC_ASSOCIATION_RETAIN);
            RELEASE_VALUE(value);
        }
        RELEASE_VALUE(target);
    }
    testassertequal(laterDeallocs, 1);
    testassertequal(normalDeallocs, otherObjsCount * 2);
}

void TestReleaseLaterRemoveAssociations(void) {

    char normalKey;
    char laterKey;

    __block int normalDeallocs = 0;
    __block int laterDeallocs = 0;

    @autoreleasepool {
        id target = [NSObject new];
        {
            id normalValue = [[CallOnDealloc alloc] initWithBlock: ^{ normalDeallocs++; }];
            id laterValue = [[CallOnDealloc alloc] initWithBlock: ^{ laterDeallocs++; }];
            objc_setAssociatedObject(target, &normalKey, normalValue, OBJC_ASSOCIATION_RETAIN);
            objc_setAssociatedObject(target, &laterKey, laterValue, (objc_AssociationPolicy)(OBJC_ASSOCIATION_RETAIN | _OBJC_ASSOCIATION_SYSTEM_OBJECT));
            RELEASE_VALUE(normalValue);
            RELEASE_VALUE(laterValue);
        }
        testassertequal(normalDeallocs, 0);
        testassertequal(laterDeallocs, 0);

        objc_removeAssociatedObjects(target);
        testassertequal(normalDeallocs, 1);
        testassertequal(laterDeallocs, 0);

        id normalValue = objc_getAssociatedObject(target, &normalKey);
        id laterValue = objc_getAssociatedObject(target, &laterKey);
        testassert(!normalValue);
        testassert(laterValue);

        RELEASE_VALUE(target);
    }

    testassertequal(normalDeallocs, 1);
    testassertequal(laterDeallocs, 1);
}

int main()
{
    testonthread(^{
        int i;
        for (i = 0; i < 100; i++) {
            RELEASE_VALUE([[Super alloc] init]);
        }
    });
    testcollect();
            
    testassert(supers == 0);
    testassert(subs > 0);
    testassert(subs == values);


    supers = 0;
    subs = 0;
    values = 0;

    testonthread(^{
        int i;
        for (i = 0; i < 100; i++) {
            RELEASE_VALUE([[Super2 alloc] init]);
        }
    });
    testcollect();

    testassert(supers > 0);
    testassert(subs == 0);
    testassert(supers == values);
    
    // rdar://44094390 tolerate nil object and nil value
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    objc_setAssociatedObject(nil, &key, nil, OBJC_ASSOCIATION_ASSIGN);
#pragma clang diagnostic pop

    // rdar://problem/59318867 Make sure we don't reenter the association lock
    // when setting an associated object on an uninitialized class.
    Class Sub59318867Local = objc_getClass("Sub59318867");
    objc_setAssociatedObject(Sub59318867Local, &key, Sub59318867Local, OBJC_ASSOCIATION_ASSIGN);

    TestReleaseLater();
    TestReleaseLaterRemoveAssociations();

    succeed(__FILE__);
}
