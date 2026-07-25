/*
Variant on fakeRealizedClass which tests a fake class with no superclass rdar://problem/67692760
TEST_CONFIG OS=macosx
TEST_CRASHES
TEST_RUN_OUTPUT
objc\[\d+\]: realized class 0x[0-9a-fA-F]+ has corrupt data pointer 0x[0-9a-fA-F]+
objc\[\d+\]: HALTED
END
*/

#include "test.h"

#include <objc/NSObject.h>

#define RW_REALIZED (1U<<31)

struct ObjCClass {
    struct ObjCClass * __ptrauth_objc_isa_pointer isa;
    struct ObjCClass * __ptrauth_objc_super_pointer superclass;
    void *cachePtr;
    uintptr_t zero;
    uintptr_t data;
};

struct ObjCClass_ro {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
#ifdef __LP64__
    uint32_t reserved;
#endif

    union {
        const uint8_t * ivarLayout;
        struct ObjCClass * nonMetaClass;
    };
    
    const char * name;
    struct ObjCMethodList * __ptrauth_objc_method_list_pointer baseMethodList;
    struct protocol_list_t * baseProtocols;
    const struct ivar_list_t * ivars;

    const uint8_t * weakIvarLayout;
    struct property_list_t *baseProperties;
};

extern struct ObjCClass OBJC_METACLASS_$_NSObject;
extern struct ObjCClass OBJC_CLASS_$_NSObject;

struct ObjCClass_ro FakeSuperclassRO = {
    .flags = RW_REALIZED
};

struct ObjCClass FakeSuperclass = {
    &OBJC_METACLASS_$_NSObject,
    NULL,
    NULL,
    0,
    (uintptr_t)&FakeSuperclassRO
};

struct ObjCClass_ro FakeSubclassRO;

struct ObjCClass FakeSubclass = {
  &FakeSuperclass,
  &FakeSuperclass,
  NULL,
  0,
  (uintptr_t)&FakeSubclassRO
};

static struct ObjCClass *class_ptr __attribute__((used)) __attribute((section("__DATA,__objc_nlclslist"))) = &FakeSubclass;

int main() {}
