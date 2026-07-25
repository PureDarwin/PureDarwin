/*
TEST_RUN_OUTPUT
LazyClassName
LazyClassName2
END
*/

#include "test.h"
#include "testroot.i"

typedef const char * _Nullable (*objc_hook_lazyClassNamer)(_Nonnull Class);

void objc_setHook_lazyClassNamer(_Nonnull objc_hook_lazyClassNamer newValue,
                                  _Nonnull objc_hook_lazyClassNamer * _Nonnull oldOutValue);

#define RW_COPIED_RO          (1<<27)

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

extern struct ObjCClass LazyClassName;
extern struct ObjCClass LazyClassName2;

struct ObjCClass_ro LazyClassNameMetaclass_ro = {
    .flags = 1,
    .instanceStart = 40,
    .instanceSize = 40,
    .nonMetaClass = &LazyClassName,
};

struct ObjCClass LazyClassNameMetaclass = {
    .isa = &OBJC_METACLASS_$_NSObject,
    .superclass = &OBJC_METACLASS_$_NSObject,
    .cachePtr = &_objc_empty_cache,
    .data = (uintptr_t)&LazyClassNameMetaclass_ro,
};

struct ObjCClass_ro LazyClassName_ro = {
    .instanceStart = 8,
    .instanceSize = 8,
};

struct ObjCClass LazyClassName = {
    .isa = &LazyClassNameMetaclass,
    .superclass = &OBJC_CLASS_$_NSObject,
    .cachePtr = &_objc_empty_cache,
    .data = (uintptr_t)&LazyClassName_ro + 2,
};

struct ObjCClass_ro LazyClassName2Metaclass_ro = {
    .flags = 1,
    .instanceStart = 40,
    .instanceSize = 40,
    .nonMetaClass = &LazyClassName2,
};

struct ObjCClass LazyClassName2Metaclass = {
    .isa = &OBJC_METACLASS_$_NSObject,
    .superclass = &OBJC_METACLASS_$_NSObject,
    .cachePtr = &_objc_empty_cache,
    .data = (uintptr_t)&LazyClassName2Metaclass_ro,
};

struct ObjCClass_ro LazyClassName2_ro = {
    .instanceStart = 8,
    .instanceSize = 8,
};

struct ObjCClass LazyClassName2 = {
    .isa = &LazyClassName2Metaclass,
    .superclass = &OBJC_CLASS_$_NSObject,
    .cachePtr = &_objc_empty_cache,
    .data = (uintptr_t)&LazyClassName2_ro + 2,
};

static objc_hook_lazyClassNamer OrigNamer;

static const char *ClassNamer(Class cls) {
    if (cls == (__bridge Class)&LazyClassName)
        return "LazyClassName";
    return OrigNamer(cls);
}

static objc_hook_lazyClassNamer OrigNamer2;

static const char *ClassNamer2(Class cls) {
    if (cls == (__bridge Class)&LazyClassName2)
        return "LazyClassName2";
    return OrigNamer2(cls);
}

__attribute__((section("__DATA,__objc_classlist,regular,no_dead_strip")))
struct ObjCClass *LazyClassNamePtr = &LazyClassName;
__attribute__((section("__DATA,__objc_classlist,regular,no_dead_strip")))
struct ObjCClass *LazyClassNamePtr2 = &LazyClassName2;

int main() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
    objc_setHook_lazyClassNamer(ClassNamer, &OrigNamer);
    objc_setHook_lazyClassNamer(ClassNamer2, &OrigNamer2);
#pragma clang diagnostic pop
  
    printf("%s\n", class_getName([(__bridge id)&LazyClassName class]));
    printf("%s\n", class_getName([(__bridge id)&LazyClassName2 class]));
}
