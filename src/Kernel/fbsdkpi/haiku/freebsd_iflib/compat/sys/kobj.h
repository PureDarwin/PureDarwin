/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_KOBJ_H_
#define _SYS_KOBJ_H_


typedef struct kobj*	kobj_t;
typedef driver_t*		kobj_class_t;
typedef const struct device_method kobj_method_t;
typedef device_method_signature_t kobjop_t;
typedef void*				kobj_ops_t;
typedef struct kobjop_desc*	kobjop_desc_t;

struct kobjop_desc {
	int32 id;
	kobj_method_t deflt; /* default implementation */
};


/* kobj */
struct kobj_ops {
	kobj_class_t cls;
};

#define KOBJ_FIELDS		\
	struct kobj_ops	ops

struct kobj {
	KOBJ_FIELDS;
};


/* this *must* be kept in sync with driver_t as defined in haiku-module.h */
#define KOBJ_CLASS_FIELDS			\
	const char* name;				\
	kobj_method_t* methods;			\
	size_t size; /* object size */


#define KOBJOPLOOKUP(OPS,OP) do {			\
	kobjop_desc_t _desc = &OP##_##desc;		\
	kobj_method_t* _ce = NULL;				\
	_ce = kobj_lookup_method(OPS.cls,		\
		NULL, _desc);						\
	_m = _ce->method;						\
} while(0)


void kobj_init(kobj_t obj, kobj_class_t cls);
kobj_method_t* kobj_lookup_method(kobj_class_t cls, kobj_method_t **cep,
	kobjop_desc_t desc);
int kobj_error_method(void);


/* we don't need these functions */
static inline void kobj_class_compile(kobj_class_t cls) {}
static inline void kobj_class_compile_static(kobj_class_t cls, kobj_ops_t ops) {}
static inline void kobj_class_free(kobj_class_t cls) {}
static inline void kobj_init_static(kobj_t obj, kobj_class_t cls) {}


#endif /* !_SYS_KOBJ_H_ */
