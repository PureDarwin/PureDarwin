/*
 * Copyright 2014-2015 iXsystems, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <mach/mach.h>
#include <xpc/launchd.h>
#include <sys/fileport.h>
#include <time.h>
#include "xpc_internal.h"

OS_OBJECT_OBJC_CLASS_DECL(xpc_object);

struct _xpc_type_s {
	const char *description;
};

typedef const struct _xpc_type_s xt;
xt _xpc_type_invalid = { "<invalid>" };
xt _xpc_type_array = { "array" };
xt _xpc_type_bool = { "bool" };
xt _xpc_type_connection = { "connection" };
xt _xpc_type_data = { "data" };
xt _xpc_type_date = { "date" };
xt _xpc_type_dictionary = { "dictionary" };
xt _xpc_type_endpoint = { "endpoint" };
xt _xpc_type_null = { "null" };
xt _xpc_type_error = { "error" };
xt _xpc_type_fd = { "file descriptor" };
xt _xpc_type_int64 = { "int64" };
xt _xpc_type_uint64 = { "uint64" };
xt _xpc_type_shmem = { "shared memory" };
xt _xpc_type_string = { "string" };
xt _xpc_type_uuid = { "UUID" };
xt _xpc_type_double = { "double" };


struct _xpc_bool_s {
	struct xpc_object object;
};

const struct _xpc_bool_s _xpc_bool_true = { .object = {
	.header = {
		.isa = &OS_xpc_object_class,
		.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
		.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
	},
	.xo_xpc_type = XPC_TYPE_BOOL,
	.xo_size = sizeof(bool),
	.xo_u = {
		.b = true
	}
} };
const struct _xpc_bool_s _xpc_bool_false = { .object = {
	.header = {
		.isa = &OS_xpc_object_class,
		.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
		.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
	},
	.xo_xpc_type = XPC_TYPE_BOOL,
	.xo_size = sizeof(bool),
	.xo_u = {
		.b = false
	}
} };
static const struct xpc_object _xpc_null_instance = {
	.header = {
		.isa = &OS_xpc_object_class,
		.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
		.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
	},
	.xo_xpc_type = XPC_TYPE_NULL,
	.xo_size = 0
};

static size_t xpc_data_hash(const uint8_t *data, size_t length);

static xpc_type_t xpc_typemap[] = {
	NULL,
	XPC_TYPE_DICTIONARY,
	XPC_TYPE_ARRAY,
	XPC_TYPE_BOOL,
	XPC_TYPE_CONNECTION,
	XPC_TYPE_ENDPOINT,
	XPC_TYPE_NULL,
	NULL,
	XPC_TYPE_INT64,
	XPC_TYPE_UINT64,
	XPC_TYPE_DATE,
	XPC_TYPE_DATA,
	XPC_TYPE_STRING,
	XPC_TYPE_UUID,
	XPC_TYPE_FD,
	XPC_TYPE_SHMEM,
	XPC_TYPE_ERROR,
	XPC_TYPE_DOUBLE
};

static const char *xpc_typestr[] = {
	"invalid",
	"dictionary",
	"array",
	"bool",
	"connection",
	"endpoint",
	"null",
	"invalid",
	"int64",
	"uint64",
	"date",
	"data",
	"string",
	"uuid",
	"fd",
	"shmem",
	"error",
	"double"
};

__private_extern__ struct xpc_object *
_xpc_prim_create(xpc_type_t type, xpc_u value, size_t size)
{

	return (_xpc_prim_create_flags(type, value, size, 0));
}

__private_extern__ struct xpc_object *
_xpc_prim_create_flags(xpc_type_t type, xpc_u value, size_t size, uint16_t flags)
{
	struct xpc_object *xo;
	xo = _os_object_alloc(&OS_xpc_object_class, sizeof(struct xpc_object) - sizeof(struct xpc_object_header));
	if (xo == NULL)
		return (NULL);

	xo->xo_size = size;
	xo->xo_xpc_type = type;
	xo->xo_flags = flags;
	xo->xo_u = value;
	xo->xo_audit_token = NULL;

	if (type == XPC_TYPE_DICTIONARY)
		TAILQ_INIT(&xo->xo_dict);

	if (type == XPC_TYPE_ARRAY)
		TAILQ_INIT(&xo->xo_array);

	return (xo);
}

xpc_object_t
xpc_null_create(void)
{
	return &_xpc_null_instance;
}

xpc_object_t
xpc_bool_create(bool value)
{
	return value ? XPC_BOOL_TRUE : XPC_BOOL_FALSE;
}

xpc_object_t
xpc_bool_create_distinct(bool value)
{
	xpc_u val;
	val.b = value;
	return _xpc_prim_create(XPC_TYPE_BOOL, val, 1);
}

bool
xpc_bool_get_value(xpc_object_t xbool)
{
	struct xpc_object *xo = xbool;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_BOOL);

	return (xo->xo_bool);
}

void
xpc_bool_set_value(xpc_object_t xbool, bool value) {
	struct xpc_object *xo = xbool;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_BOOL);
	xpc_assert(xo->header.ref_cnt != _OS_OBJECT_GLOBAL_REFCNT, "You cannot call xpc_bool_set_value() on the statically allocated xpc_bool instances");

	xo->xo_bool = value;
}

xpc_object_t
xpc_int64_create(int64_t value)
{
	xpc_u val;

	val.i = value;
	return _xpc_prim_create(XPC_TYPE_INT64, val, 1);
}

int64_t
xpc_int64_get_value(xpc_object_t xint)
{
	struct xpc_object *xo = xint;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_INT64);

	return (xo->xo_int);
}

void
xpc_int64_set_value(xpc_object_t xint, int64_t value) {
	struct xpc_object *xo = xint;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_INT64);

	xo->xo_int = value;
}

xpc_object_t
xpc_uint64_create(uint64_t value)
{
	xpc_u val;

	val.ui = value;
	return _xpc_prim_create(XPC_TYPE_UINT64, val, 1);
}

uint64_t
xpc_uint64_get_value(xpc_object_t xuint)
{
	struct xpc_object *xo = xuint;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_UINT64);

	return (xo->xo_uint);
}

void
xpc_uint64_set_value(xpc_object_t xuint, uint64_t value)
{
	struct xpc_object *xo = xuint;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_UINT64);

	xo->xo_uint = value;
}

xpc_object_t
xpc_double_create(double value)
{
	xpc_u val;

	val.d = value;
	return _xpc_prim_create(XPC_TYPE_DOUBLE, val, 1);
}

double
xpc_double_get_value(xpc_object_t xdouble)
{
	struct xpc_object *xo = xdouble;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DOUBLE);

	return (xo->xo_d);
}

void
xpc_double_set_value(xpc_object_t xdouble, double value)
{
	struct xpc_object *xo = xdouble;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DOUBLE);

	xo->xo_d = value;
}

xpc_object_t
xpc_date_create(int64_t interval)
{
	xpc_u val;

	val.i = interval;
	return _xpc_prim_create(XPC_TYPE_DATE, val, 1);
}

xpc_object_t
xpc_date_create_from_current(void)
{
    return xpc_date_create(time(NULL) * NSEC_PER_SEC);
}

int64_t
xpc_date_get_value(xpc_object_t xdate)
{
	struct xpc_object *xo = xdate;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DATE);

	return (xo->xo_int);
}

xpc_object_t
xpc_data_create(const void *bytes, size_t length)
{
	xpc_u val;

	val.ptr = (uintptr_t)malloc(length);
	memcpy((void *)val.ptr, bytes, length);
	return _xpc_prim_create(XPC_TYPE_DATA, val, length);
}

xpc_object_t
xpc_data_create_with_dispatch_data(dispatch_data_t ddata)
{
	const void *buffer;
	size_t size;
	dispatch_data_t mapped;
	xpc_object_t xdata;

	buffer = NULL;
	size = 0;
	mapped = dispatch_data_create_map(ddata, &buffer, &size);
	if (mapped == NULL)
		return (NULL);

	xdata = xpc_data_create(buffer, size);
	dispatch_release(mapped);
	return (xdata);
}

size_t
xpc_data_get_length(xpc_object_t xdata)
{
	struct xpc_object *xo = xdata;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DATA);

	return (xo->xo_size);
}

const void *
xpc_data_get_bytes_ptr(xpc_object_t xdata)
{
	struct xpc_object *xo = xdata;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DATA);

	return (void *)(xo->xo_ptr);
}

size_t
xpc_data_get_bytes(xpc_object_t xdata, void *buffer, size_t off, size_t length)
{
	struct xpc_object *xo = xdata;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DATA);

	if (off >= xo->xo_size)
		return 0;

	size_t length_to_copy = length;
	if (length_to_copy > xo->xo_size - off)
		length_to_copy = xo->xo_size - off;

	memcpy(buffer, (void *)(xo->xo_u.ptr + off), length_to_copy);

	return length_to_copy;
}

void
xpc_data_set_bytes(xpc_object_t xdata, void *buffer, size_t length)
{
	struct xpc_object *xo = xdata;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DATA);

	free((void *)xo->xo_u.ptr);
	xo->xo_u.ptr = (uintptr_t)malloc(length);
	memcpy((void *)xo->xo_u.ptr, buffer, length);
	xo->xo_size = length;
}

xpc_object_t
xpc_fd_create(int fd)
{
	xpc_u val;
	fileport_makeport(fd, &val.port);
	return _xpc_prim_create(XPC_TYPE_FD, val, sizeof(val.port));
}

int
xpc_fd_dup(xpc_object_t xfd)
{
	struct xpc_object *xo = xfd;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_FD);

	return fileport_makefd(xo->xo_u.port);
}

xpc_object_t
xpc_string_create(const char *string)
{
	xpc_u val;

	val.str = strdup(string);
	return _xpc_prim_create(XPC_TYPE_STRING, val, strlen(string));
}

xpc_object_t
xpc_string_create_with_format(const char *fmt, ...)
{
	va_list ap;
	xpc_u val;

	va_start(ap, fmt);
	vasprintf((char **)&val.str, fmt, ap);
	va_end(ap);
	return _xpc_prim_create(XPC_TYPE_STRING, val, strlen(val.str));
}

xpc_object_t
xpc_string_create_with_format_and_arguments(const char *fmt, va_list ap)
{
	xpc_u val;

	vasprintf((char **)&val.str, fmt, ap);
	return _xpc_prim_create(XPC_TYPE_STRING, val, strlen(val.str));
}

size_t
xpc_string_get_length(xpc_object_t xstring)
{
	struct xpc_object *xo = xstring;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_STRING);

	return (xo->xo_size);
}

const char *
xpc_string_get_string_ptr(xpc_object_t xstring)
{
	struct xpc_object *xo = xstring;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_STRING);

	return (xo->xo_str);
}

void
xpc_string_set_value(xpc_object_t xstring, const char *value) {
	struct xpc_object *xo = xstring;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_STRING);

	free(xo->xo_u.str);
	xo->xo_u.str = strdup(value);
}

xpc_object_t
xpc_uuid_create(const uuid_t uuid)
{
	xpc_u val;

	memcpy(val.uuid, uuid, sizeof(uuid_t));
	return _xpc_prim_create(XPC_TYPE_UUID, val, 1);
}

const uint8_t *
xpc_uuid_get_bytes(xpc_object_t xuuid)
{
	struct xpc_object *xo = xuuid;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_UUID);

	return ((uint8_t*)&xo->xo_uuid);
}

xpc_type_t
xpc_get_type(xpc_object_t obj)
{
	struct xpc_object *xo;

	xo = obj;
	return xo->xo_xpc_type;
}

bool
xpc_equal(xpc_object_t x1, xpc_object_t x2)
{
	struct xpc_object *xo1, *xo2;

	xo1 = x1;
	xo2 = x2;

	xpc_assert_nonnull(xo1);
	xpc_assert_nonnull(xo2);

	if (xo1->xo_xpc_type != xo2->xo_xpc_type) return false;

	if (xo1->xo_xpc_type == XPC_TYPE_BOOL) {
		return xo1->xo_u.b == xo2->xo_u.b;
	} else if (xo1->xo_xpc_type == XPC_TYPE_INT64 || xo1->xo_xpc_type == XPC_TYPE_DATE) {
		return xo1->xo_u.i == xo2->xo_u.i;
	} else if (xo1->xo_xpc_type == XPC_TYPE_UINT64) {
		return xo1->xo_u.ui == xo2->xo_u.ui;
	} else if (xo1->xo_xpc_type == XPC_TYPE_ENDPOINT) {
		return xo1->xo_u.port == xo2->xo_u.port;
	} else if (xo1->xo_xpc_type == XPC_TYPE_STRING) {
		return strcmp(xo1->xo_u.str, xo2->xo_u.str) == 0;
	} else if (xo1->xo_xpc_type == XPC_TYPE_DATA) {
		if (xo1->xo_size != xo2->xo_size) return false;
		return memcmp((void *)xo1->xo_u.ptr, (void *)xo2->xo_u.ptr, xo1->xo_size) == 0;
	} else if (xo1->xo_xpc_type == XPC_TYPE_DICTIONARY) {
		struct xpc_dict_pair *pair;
		size_t count1 = 0, count2 = 0;
		TAILQ_FOREACH(pair, &xo1->xo_u.dict, xo_link) {
			count1++;
		}
		TAILQ_FOREACH(pair, &xo2->xo_u.dict, xo_link) {
			count2++;
		}

		if (count1 != count2) return false;

		TAILQ_FOREACH(pair, &xo1->xo_u.dict, xo_link) {
			struct xpc_object *value1 = pair->value;
			struct xpc_object *value2 = xpc_dictionary_get_value(xo2, pair->key);
			if (value2 == NULL) return false;
			if (!xpc_equal(value1, value2)) return false;
		}

		TAILQ_FOREACH(pair, &xo2->xo_u.dict, xo_link) {
			if (xpc_dictionary_get_value(xo1, pair->key) == NULL) return false;
		}

		return true;
	} else if (xo1->xo_xpc_type == XPC_TYPE_ARRAY) {
		if (xpc_array_get_count(xo1) != xpc_array_get_count(xo2)) return false;

		return xpc_array_apply(xo1, ^bool(size_t index, xpc_object_t value) {
			return xpc_equal(value, xpc_array_get_value(xo2, index));
		});
	} else {
		xpc_api_misuse("xpc_equal() is not implemented for this object type");
	}
}

static size_t
xpc_data_hash(const uint8_t *data, size_t length)
{
    size_t hash = 5381;

    while (length--)
        hash = ((hash << 5) + hash) + data[length];

    return (hash);
}

size_t
xpc_hash(xpc_object_t obj)
{
	struct xpc_object *xo;
	__block size_t hash = 0;

	xo = obj;
	if (xo->xo_xpc_type == XPC_TYPE_BOOL || xo->xo_xpc_type == XPC_TYPE_INT64 ||
		xo->xo_xpc_type == XPC_TYPE_UINT64 || xo->xo_xpc_type == XPC_TYPE_DATE ||
		xo->xo_xpc_type == XPC_TYPE_ENDPOINT) {
		return ((size_t)xo->xo_u.port);
	} else if (xo->xo_xpc_type == XPC_TYPE_STRING) {
		return (xpc_data_hash((const uint8_t *)xpc_string_get_string_ptr(obj), xpc_string_get_length(obj)));
	} else if (xo->xo_xpc_type == XPC_TYPE_DATA) {
		return (xpc_data_hash(xpc_data_get_bytes_ptr(obj), xpc_data_get_length(obj)));
	} else if (xo->xo_xpc_type == XPC_TYPE_DICTIONARY) {
		xpc_dictionary_apply(obj, ^bool(const char *k, xpc_object_t v) {
			hash ^= xpc_data_hash((const uint8_t *)k, strlen(k));
			hash ^= xpc_hash(v);
			return true;
		});
		return (hash);
	} else if (xo->xo_xpc_type == XPC_TYPE_ARRAY) {
		xpc_array_apply(obj, ^bool(size_t idx, xpc_object_t v) {
			hash ^= xpc_hash(v);
			return true;
		});
		return (hash);
	}

	return (xpc_data_hash((const uint8_t *)_xpc_get_type_name(obj),
	    strlen(_xpc_get_type_name(obj))));
}

mach_port_t
xpc_object_get_machport(xpc_object_t object)
{
	struct xpc_object *xo = object;

	xpc_assert_nonnull(xo);
	xpc_assert(xo->xo_xpc_type == XPC_TYPE_CONNECTION || xo->xo_xpc_type == XPC_TYPE_ENDPOINT,
	    "object is not a connection or endpoint");

	if (xo->xo_xpc_type == XPC_TYPE_CONNECTION)
		return ((struct xpc_connection *)object)->xc_remote_port;

	return xo->xo_u.port;
}

__private_extern__ const char *
_xpc_get_type_name(xpc_object_t obj)
{
	struct xpc_object *xo;

	xo = obj;
	return xo->xo_xpc_type->description;
}
