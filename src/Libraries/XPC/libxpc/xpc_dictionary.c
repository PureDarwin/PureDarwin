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
#include "xpc_internal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define NVLIST_XPC_TYPE         XPC_RESERVED_KEY_PREFIX "object type"
#define NVLIST_PORT_INDEX		XPC_RESERVED_KEY_PREFIX "port index"

static void xpc2nv_primitive(nvlist_t *nv, const char *key, xpc_object_t value, int64_t (^port_serializer)(mach_port_t port));

__private_extern__ void
nv_release_entry(nvlist_t *nv, const char *key)
{
	xpc_object_t tmp;

	if (nvlist_exists_type(nv, key, NV_TYPE_PTR)) {
		tmp = (void *)nvlist_take_number(nv, key);
		xpc_release(tmp);
	}
}

struct xpc_object *
nv2xpc(const nvlist_t *nv, mach_port_t (^port_deserializer)(int64_t port_id))
{
	struct xpc_object *xo = NULL, *xotmp = NULL;
	void *cookiep;
	const char *key;
	int type;
	xpc_u val;
	const nvlist_t *nvtmp;

	xpc_assert(nv != NULL, "%s: nvlist_t is NULL", __FUNCTION__);
	xpc_assert(nvlist_type(nv) == NV_TYPE_NVLIST_DICTIONARY || nvlist_type(nv) == NV_TYPE_NVLIST_ARRAY, "nvlist_t %p is not dictionary or array", nv);

	if (nvlist_type(nv) == NV_TYPE_NVLIST_DICTIONARY) {
		if (nvlist_contains_key(nv, NVLIST_XPC_TYPE)) {
			const char *type = nvlist_get_string(nv, NVLIST_XPC_TYPE);

			if (strcmp(type, "connection") == 0) {
				int64_t port_id = nvlist_get_int64(nv, NVLIST_PORT_INDEX);
				val.port = port_deserializer(port_id);
				return _xpc_prim_create(XPC_TYPE_CONNECTION, val, 0);
			} else if (strcmp(type, "endpoint") == 0) {
				int64_t port_id = nvlist_get_int64(nv, NVLIST_PORT_INDEX);
				val.port = port_deserializer(port_id);
				return _xpc_prim_create(XPC_TYPE_ENDPOINT, val, 0);
			} else if (strcmp(type, "fileport") == 0) {
				int64_t port_id = nvlist_get_int64(nv, NVLIST_PORT_INDEX);
				val.port = port_deserializer(port_id);
				return _xpc_prim_create(XPC_TYPE_FD, val, 0);
			} else if (strcmp(type, "date") == 0) {
				return xpc_date_create(nvlist_get_int64(nv, "date"));
			} else if (strcmp(type, "double") == 0) {
				size_t value_size;
				double *value = (double *)nvlist_get_binary(nv, "date", &value_size);
				xpc_assert(value_size == sizeof(double), "nvlist data of type date has incorrect size (expected %lu, got %zu)", sizeof(double), value_size);
				return xpc_double_create(*value);
			} else {
				xpc_api_misuse("Unexpected NVLIST_XPC_TYPE in dictionary: %s", type);
			}
		}

		xo = xpc_dictionary_create(NULL, NULL, 0);
	}

	if (nvlist_type(nv) == NV_TYPE_NVLIST_ARRAY)
		xo = xpc_array_create(NULL, 0);

	cookiep = NULL;
	while ((key = nvlist_next(nv, &type, &cookiep)) != NULL) {
		xotmp = NULL;

		switch (type) {
		case NV_TYPE_BOOL:
			val.b = nvlist_get_bool(nv, key);
			xotmp = xpc_bool_create(val.b);
			break;

		case NV_TYPE_STRING:
			val.str = nvlist_get_string(nv, key);
			xotmp = xpc_string_create(val.str);
			break;

		case NV_TYPE_INT64:
			val.i = nvlist_get_int64(nv, key);
			xotmp = xpc_int64_create(val.i);
			break;

		case NV_TYPE_UINT64:
			val.ui = nvlist_get_uint64(nv, key);
			xotmp = xpc_uint64_create(val.ui);
			break;

		case NV_TYPE_DESCRIPTOR:
			xpc_api_misuse("NV_TYPE_DESCRIPTOR should not appear in received nvlist_t");
			break;

		case NV_TYPE_PTR:
			break;

		case NV_TYPE_BINARY:
			{
				size_t value_size;
				const void *value;

				value = nvlist_get_binary(nv, key, &value_size);
				xotmp = xpc_data_create(value, value_size);
			}
			break;

		case NV_TYPE_UUID:
			memcpy(&val.uuid, nvlist_get_uuid(nv, key),
			    sizeof(uuid_t));
			xotmp = _xpc_prim_create(XPC_TYPE_UUID, val, 0);
			break;

		case NV_TYPE_NVLIST_ARRAY:
			nvtmp = nvlist_get_nvlist_array(nv, key);
			xotmp = nv2xpc(nvtmp, port_deserializer);
			break;

		case NV_TYPE_NVLIST_DICTIONARY:
			nvtmp = nvlist_get_nvlist_dictionary(nv, key);
			xotmp = nv2xpc(nvtmp, port_deserializer);
			break;
		}

		if (xotmp) {
			if (nvlist_type(nv) == NV_TYPE_NVLIST_DICTIONARY)
				xpc_dictionary_set_value(xo, key, xotmp);

			if (nvlist_type(nv) == NV_TYPE_NVLIST_ARRAY)
				xpc_array_append_value(xo, xotmp);
		}
	}

	return (xo);
}

static void
xpc2nv_primitive(nvlist_t *nv, const char *key, xpc_object_t value, int64_t (^port_serializer)(mach_port_t port))
{
	struct xpc_object *xotmp = value;
	nvlist_t *inner_nv;

	if (xotmp->xo_xpc_type == XPC_TYPE_DICTIONARY) {
		nvlist_add_nvlist_dictionary(nv, key, xpc2nv(xotmp, port_serializer));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_ARRAY) {
		nvlist_add_nvlist_array(nv, key, xpc2nv(xotmp, port_serializer));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_BOOL) {
		nvlist_add_bool(nv, key, xpc_bool_get_value(xotmp));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_CONNECTION) {
		inner_nv = nvlist_create_dictionary(0);
		nvlist_add_string(inner_nv, NVLIST_XPC_TYPE, "connection");
		nvlist_add_int64(inner_nv, NVLIST_PORT_INDEX, port_serializer(xotmp->xo_port));
		nvlist_add_nvlist(nv, key, inner_nv);
		nvlist_destroy(inner_nv);
	} else if (xotmp->xo_xpc_type == XPC_TYPE_ENDPOINT) {
		inner_nv = nvlist_create_dictionary(0);
		nvlist_add_string(inner_nv, NVLIST_XPC_TYPE, "endpoint");
		nvlist_add_int64(inner_nv, NVLIST_PORT_INDEX, port_serializer(xotmp->xo_port));
		nvlist_add_nvlist(nv, key, inner_nv);
		nvlist_destroy(inner_nv);
	} else if (xotmp->xo_xpc_type == XPC_TYPE_INT64) {
		nvlist_add_int64(nv, key, xpc_int64_get_value(xotmp));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_UINT64) {
		nvlist_add_uint64(nv, key,  xpc_uint64_get_value(xotmp));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_DATE) {
		inner_nv = nvlist_create_dictionary(0);
		nvlist_add_string(inner_nv, NVLIST_XPC_TYPE, "date");
		nvlist_add_int64(inner_nv, "date", xotmp->xo_u.i);
		nvlist_add_nvlist(nv, key, inner_nv);
		nvlist_destroy(inner_nv);
	} else if (xotmp->xo_xpc_type == XPC_TYPE_DATA) {
		nvlist_add_binary(nv, key, xpc_data_get_bytes_ptr(xotmp), xpc_data_get_length(xotmp));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_STRING) {
		nvlist_add_string(nv, key, xpc_string_get_string_ptr(xotmp));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_UUID) {
		nvlist_add_uuid(nv, key, (uuid_t*)xpc_uuid_get_bytes(xotmp));
	} else if (xotmp->xo_xpc_type == XPC_TYPE_FD) {
		inner_nv = nvlist_create_dictionary(0);
		nvlist_add_string(inner_nv, NVLIST_XPC_TYPE, "fileport");
		nvlist_add_int64(inner_nv, NVLIST_PORT_INDEX, port_serializer(xotmp->xo_port));
		nvlist_add_nvlist(nv, key, inner_nv);
		nvlist_destroy(inner_nv);
	} else if (xotmp->xo_xpc_type == XPC_TYPE_SHMEM) {
		xpc_api_misuse("Cannot serialize object of type shared memory");
	} else if (xotmp->xo_xpc_type == XPC_TYPE_ERROR) {
		xpc_api_misuse("Cannot serialize object of type error");
	} else if (xotmp->xo_xpc_type == XPC_TYPE_DOUBLE) {
		inner_nv = nvlist_create_dictionary(0);
		nvlist_add_string(inner_nv, NVLIST_XPC_TYPE, "double");
		nvlist_add_binary(inner_nv, "double", &xotmp->xo_u.d, sizeof(double));
		nvlist_add_nvlist(nv, key, inner_nv);
		nvlist_destroy(inner_nv);
	} else {
		xpc_api_misuse("Unknown XPC type for object");
	}
}

nvlist_t *
xpc2nv(struct xpc_object *xo, int64_t (^port_serializer)(mach_port_t port))
{
	nvlist_t *nv;
	struct xpc_object *xotmp;

	if (xo->xo_xpc_type == XPC_TYPE_DICTIONARY) {
		nv = nvlist_create_dictionary(0);
		debugf("nv = %p\n", nv);
		xpc_dictionary_apply(xo, ^(const char *k, xpc_object_t v) {
			xpc2nv_primitive(nv, k, v, port_serializer);
			return ((bool)true);
		});

		return nv;
	}

	if (xo->xo_xpc_type == XPC_TYPE_ARRAY) {
		char *key = NULL;
		nv = nvlist_create_array(0);
		xpc_array_apply(xo, ^(size_t index, xpc_object_t v) {
			asprintf(&key, "%ld", index);
			xpc2nv_primitive(nv, key, v, port_serializer);
			free(key);
			return ((bool)true);
		});

		return nv;
	}

	xpc_assert(0, "xpc_object not of %s type", "array or dictionary");
	return NULL;
}

xpc_object_t
xpc_dictionary_create(const char * const *keys, const xpc_object_t *values,
    size_t count)
{
	struct xpc_object *xo;
	size_t i;
	xpc_u val = {0};

	xo = _xpc_prim_create(XPC_TYPE_DICTIONARY, val, count);
	
	for (i = 0; i < count; i++)
		xpc_dictionary_set_value(xo, keys[i], values[i]);
	
	return (xo);
}

xpc_object_t
xpc_dictionary_create_reply(xpc_object_t original)
{
	struct xpc_object *xo, *xo_orig;
	nvlist_t *nv;
	xpc_u val;

	xo_orig = original;
	if ((xo_orig->xo_flags & _XPC_FROM_WIRE) == 0)
		return (NULL);

	xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);

	mach_port_t rport = xpc_dictionary_copy_mach_send(original, XPC_RPORT);
	if (rport != MACH_PORT_NULL) xpc_dictionary_set_mach_send(reply, XPC_RPORT, rport);
	uint64_t seqid = xpc_dictionary_get_uint64(original, XPC_SEQID);
	if (seqid != 0) xpc_dictionary_set_uint64(reply, XPC_SEQID, seqid);

	return reply;
}

void
xpc_dictionary_get_audit_token(xpc_object_t xdict, audit_token_t *token)
{
	struct xpc_object *xo;
	xpc_assert_nonnull(xdict);

	xo = xdict;
	if (xo->xo_audit_token != NULL)
		memcpy(token, xo->xo_audit_token, sizeof(*token));
}
void
xpc_dictionary_set_mach_recv(xpc_object_t xdict, const char *key, mach_port_t port)
{
	struct xpc_object *xo = xdict;
	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	struct xpc_object *xotmp;
	xpc_u val;

	val.port = port;
	xotmp = _xpc_prim_create(XPC_TYPE_ENDPOINT, val, 0);

	xpc_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_dictionary_set_mach_send(xpc_object_t xdict, const char *key, mach_port_t port)
{
	struct xpc_object *xotmp;
	xpc_u val;

	val.port = port;
	xotmp = _xpc_prim_create(XPC_TYPE_ENDPOINT, val, 0);

	xpc_dictionary_set_value(xdict, key, xotmp);
}

mach_port_t
xpc_dictionary_copy_mach_send(xpc_object_t xdict, const char *key)
{
	struct xpc_object *xo = xdict;

	xpc_assert_nonnull(xdict);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return MACH_PORT_NULL;

	struct xpc_object *xovalue = value;
	xpc_assert_type(xovalue, XPC_TYPE_ENDPOINT);

	return xovalue->xo_port;
}

void
xpc_dictionary_set_value_nokeycheck(xpc_object_t xdict, const char *key, xpc_object_t value)
{
	struct xpc_object *xo = xdict, *xotmp;
	struct xpc_dict_head *head;
	struct xpc_dict_pair *pair;

	xpc_assert_nonnull(xdict);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!strcmp(pair->key, key)) {
			if (pair->value != NULL)
				xpc_release(pair->value);
			if (value != NULL) {
				xpc_retain(value);
				pair->value = value;
				} else {
					TAILQ_REMOVE(head, pair, xo_link);
					xo->xo_size--;
					free((void *)pair->key);
					free(pair);
				}

				return;
			}
		}

	if (value == NULL) {
		return;
	}

	xo->xo_size++;
	pair = malloc(sizeof(struct xpc_dict_pair));
	if (pair == NULL) {
		return;
	}
	pair->key = strdup(key);
	if (pair->key == NULL) {
		free(pair);
		return;
	}
	pair->value = value;
	TAILQ_INSERT_TAIL(&xo->xo_dict, pair, xo_link);
	xpc_retain(value);
}

void
xpc_dictionary_set_value(xpc_object_t xdict, const char *key, xpc_object_t value) {
	bool is_reserved_key = strncmp(key, NVLIST_XPC_TYPE, strlen(NVLIST_XPC_TYPE)) == 0;
	xpc_precondition(!is_reserved_key, "Cannot add key %s to dictionary, as it is reserved for internal use", key);
	xpc_dictionary_set_value_nokeycheck(xdict, key, value);

}

xpc_object_t
xpc_dictionary_get_value(xpc_object_t xdict, const char *key)
{
	xpc_assert_nonnull(xdict);

	struct xpc_object *xo;
	struct xpc_dict_head *head;
	struct xpc_dict_pair *pair;

	xo = xdict;
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!strcmp(pair->key, key))
			return (pair->value);
	}

	return (NULL);
}

size_t
xpc_dictionary_get_count(xpc_object_t xdict)
{
	xpc_assert_nonnull(xdict);

	struct xpc_object *xo;

	xo = xdict;
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);
	return (xo->xo_size);
}

void
xpc_dictionary_set_bool(xpc_object_t xdict, const char *key, bool value)
{;
	struct xpc_object *xo = xdict, *xotmp;

	xpc_assert_nonnull(xdict);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	xo = xdict;
	xotmp = xpc_bool_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_dictionary_set_int64(xpc_object_t xdict, const char *key, int64_t value)
{
	struct xpc_object *xo = xdict, *xotmp;

	xpc_assert_nonnull(xdict);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	xo = xdict;
	xotmp = xpc_int64_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

void
xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key, uint64_t value)
{
	struct xpc_object *xo = xdict, *xotmp;

	xpc_assert_nonnull(xdict);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	xo = xdict;
	xotmp = xpc_uint64_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp); 
	xpc_release(xotmp);
}

void
xpc_dictionary_set_double(xpc_object_t xdict, const char *key, double value)
{
	struct xpc_object *xotmp = xpc_double_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

void
xpc_dictionary_set_date(xpc_object_t xdict, const char *key, int64_t value)
{
	struct xpc_object *xotmp = xpc_date_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

void
xpc_dictionary_set_data(xpc_object_t xdict, const char *key, const void *bytes,
    size_t length)
{
	struct xpc_object *xotmp = xpc_data_create(bytes, length);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

void
xpc_dictionary_set_string(xpc_object_t xdict, const char *key, const char *value)
{
	struct xpc_object *xotmp = xpc_string_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

void
xpc_dictionary_set_uuid(xpc_object_t xdict, const char *key, const uuid_t uuid)
{
	struct xpc_object *xotmp = xpc_uuid_create(uuid);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

void
xpc_dictionary_set_fd(xpc_object_t xdict, const char *key, int fd)
{
	struct xpc_object *xotmp = xpc_fd_create(fd);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

bool
xpc_dictionary_get_bool(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return FALSE;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_BOOL) return FALSE;

	return (xpc_bool_get_value(value));
}

int64_t
xpc_dictionary_get_int64(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return 0;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_INT64) return 0;

	return (xpc_int64_get_value(value));
}

uint64_t
xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return 0;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_UINT64) return 0;

	return (xpc_uint64_get_value(value));
}

double
xpc_dictionary_get_double(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return 0;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_DOUBLE) return 0;

	return (xpc_double_get_value(value));
}

int64_t
xpc_dictionary_get_date(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return 0;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_DATE) return 0;

	return (xpc_date_get_value(value));
}

const char *
xpc_dictionary_get_string(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return 0;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_STRING) return NULL;

	return (xpc_string_get_string_ptr(value));
}

const void *
xpc_dictionary_get_data(xpc_object_t xdict, const char *key, size_t *length)
{
	xpc_object_t xdata = xpc_dictionary_get_value(xdict, key);
	if (xdata == NULL) return NULL;

	struct xpc_object *xo = xdata;
	if (xo->xo_xpc_type != XPC_TYPE_DATA) return NULL;

	if (length != NULL) *length = xpc_data_get_length(xdata);
	return xpc_data_get_bytes_ptr(xdata);
}

const uint8_t *
xpc_dictionary_get_uuid(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return NULL;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_UUID) return NULL;

	return (xpc_uuid_get_bytes(value));
}

int
xpc_dictionary_dup_fd(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL) return -1;

	struct xpc_object *xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_FD) return -1;

	return (xpc_fd_dup(value));
}

void
xpc_dictionary_set_connection(xpc_object_t xdict, const char *key,
    xpc_connection_t connection)
{
	xpc_endpoint_t endpoint;

	endpoint = xpc_endpoint_create(connection);
	xpc_dictionary_set_value(xdict, key, endpoint);
	xpc_release(endpoint);
}

xpc_connection_t
xpc_dictionary_create_connection(xpc_object_t xdict, const char *key)
{
	xpc_object_t value;
	struct xpc_object *xo;

	value = xpc_dictionary_get_value(xdict, key);
	if (value == NULL)
		return (NULL);

	xo = value;
	if (xo->xo_xpc_type != XPC_TYPE_ENDPOINT)
		return (NULL);

	return (xpc_connection_create_from_endpoint(value));
}

bool
xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier)
{
	struct xpc_object *xo = xdict, *xotmp;
	struct xpc_dict_head *head;
	struct xpc_dict_pair *pair;

	xpc_assert_nonnull(xdict);
	xpc_assert_type(xo, XPC_TYPE_DICTIONARY);

	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!applier(pair->key, pair->value))
			return (false);
	}

	return (true);
}
