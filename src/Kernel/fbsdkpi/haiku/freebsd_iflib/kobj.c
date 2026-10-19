/*
 * Copyright 2018-2019, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/mutex.h>


static kobj_method_t kobj_default_method = {
	"kobj_default_method", 0, (kobjop_t)kobj_error_method
};


void
kobj_init(kobj_t obj, kobj_class_t cls)
{
	obj->ops.cls = cls;
}


kobj_method_t*
kobj_lookup_method(kobj_class_t class, kobj_method_t** cep,
	kobjop_desc_t desc)
{
	int32 i, id;
	kobj_method_t* ret = NULL;

	id = desc->id;
	if (id == 0)
		id = desc->deflt.id;

	for (i = 0; class->methods[i].name != NULL; i++) {
		kobj_method_t* mth = &class->methods[i];
		if (mth->id != id)
			continue;

		ret = mth;
		break;
	}
	if (ret == NULL || ret->method == NULL)
		ret = &desc->deflt;
	if (ret == NULL || ret->method == NULL)
		ret = &kobj_default_method;

	if (cep != NULL)
		*cep = ret;
	return ret;
}


int
kobj_error_method(void)
{
	return ENXIO;
}
