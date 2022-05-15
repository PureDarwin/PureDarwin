/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

struct objc_selector; struct objc_class;
#ifndef OBJC_SUPER
struct objc_super { struct objc_object *object; struct objc_object *superClass; objc_super(struct objc_object *o, struct objc_object *s) : object(o), superClass(s) {} };
#define OBJC_SUPER
#endif
#ifndef _REWRITER_typedef_Protocol
typedef struct objc_object Protocol;
#define _REWRITER_typedef_Protocol
#endif
#define __OBJC_RW_EXTERN extern "C" __declspec(dllimport)
__OBJC_RW_EXTERN struct objc_object *objc_msgSend(struct objc_object *, struct objc_selector *, ...);
__OBJC_RW_EXTERN struct objc_object *objc_msgSendSuper(struct objc_super *, struct objc_selector *, ...);
__OBJC_RW_EXTERN struct objc_object *objc_msgSend_stret(struct objc_object *, struct objc_selector *, ...);
__OBJC_RW_EXTERN struct objc_object *objc_msgSendSuper_stret(struct objc_super *, struct objc_selector *, ...);
__OBJC_RW_EXTERN double objc_msgSend_fpret(struct objc_object *, struct objc_selector *, ...);
__OBJC_RW_EXTERN struct objc_object *objc_getClass(const char *);
__OBJC_RW_EXTERN struct objc_object *objc_getMetaClass(const char *);
__OBJC_RW_EXTERN void objc_exception_throw(struct objc_object *);
__OBJC_RW_EXTERN void objc_exception_try_enter(void *);
__OBJC_RW_EXTERN void objc_exception_try_exit(void *);
__OBJC_RW_EXTERN struct objc_object *objc_exception_extract(void *);
__OBJC_RW_EXTERN int objc_exception_match(struct objc_class *, struct objc_object *);
__OBJC_RW_EXTERN void objc_sync_enter(struct objc_object *);
__OBJC_RW_EXTERN void objc_sync_exit(struct objc_object *);
__OBJC_RW_EXTERN Protocol *objc_getProtocol(const char *);
#ifndef __FASTENUMERATIONSTATE
struct __objcFastEnumerationState {
	unsigned long state;
	void **itemsPtr;
	unsigned long *mutationsPtr;
	unsigned long extra[5];
};
__OBJC_RW_EXTERN void objc_enumerationMutation(struct objc_object *);
#define __FASTENUMERATIONSTATE
#endif
#ifndef __NSCONSTANTSTRINGIMPL
struct __NSConstantStringImpl {
  int *isa;
  int flags;
  char *str;
  long length;
};
#ifdef CF_EXPORT_CONSTANT_STRING
extern "C" __declspec(dllexport) int __CFConstantStringClassReference[];
#else
__OBJC_RW_EXTERN int __CFConstantStringClassReference[];
#endif
#define __NSCONSTANTSTRINGIMPL
#endif
#ifndef BLOCK_IMPL
#define BLOCK_IMPL
struct __block_impl {
  void *isa;
  int Flags;
  int Size;
  void *FuncPtr;
};
enum {
  BLOCK_HAS_COPY_DISPOSE = (1<<25),
  BLOCK_IS_GLOBAL = (1<<28)
};
// Runtime copy/destroy helper functions
__OBJC_RW_EXTERN void _Block_copy_assign(void *, void *);
__OBJC_RW_EXTERN void _Block_byref_assign_copy(void *, void *);
__OBJC_RW_EXTERN void _Block_destroy(void *);
__OBJC_RW_EXTERN void _Block_byref_release(void *);
__OBJC_RW_EXTERN void *_NSConcreteGlobalBlock;
__OBJC_RW_EXTERN void *_NSConcreteStackBlock;
#endif
#undef __OBJC_RW_EXTERN
#define __attribute__(X)
// ..\clang -rewrite-objc -fms-extensions simpleblock.c



#include <iostream>
using namespace std;
#include "Block.h"




struct __main_block_impl_0 {
  struct __block_impl impl;
  __main_block_impl_0(void *fp, int flags=0) {
    impl.isa = 0/*&_NSConcreteStackBlock*/;
    impl.Size = sizeof(__main_block_impl_0);
    impl.Flags = flags;
    impl.FuncPtr = fp;
  }
};
static void __main_block_func_0(struct __main_block_impl_0 *__cself, int x) {


	cout << "Hello, " << x << endl;

    }
int main(int argc, char **argv) {

    void(*aBlock)(int x);

    void(*bBlock)(int x);



    aBlock = (void (*)(int))&__main_block_impl_0((void *)__main_block_func_0);



    ((void (*)(struct __block_impl *, int))((struct __block_impl *)aBlock)->FuncPtr)((struct __block_impl *)aBlock, 42);



    bBlock = (void *)Block_copy(aBlock);



    ((void (*)(struct __block_impl *, int))((struct __block_impl *)bBlock)->FuncPtr)((struct __block_impl *)bBlock, 46);



    Block_release(bBlock);



    return 0;

}

