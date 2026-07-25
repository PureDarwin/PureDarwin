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
#include <sys/errno.h>
#include <sys/sbuf.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <xpc/launchd.h>
#include <assert.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <stdatomic.h>

#include "xpc_internal.h"

#define MAX_RECV 8192
#define XPC_RECV_SIZE			\
    MAX_RECV - 				\
    sizeof(mach_msg_header_t) - 	\
    sizeof(mach_msg_trailer_t) - 	\
    sizeof(uint64_t) - 			\
    sizeof(size_t)

struct xpc_message {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_ool_descriptor_t ool_data;
	mach_msg_ool_ports_descriptor_t ool_ports;
	uint64_t id;
	mach_msg_trailer_t trailer;
};

struct xpc_port_set {
	mach_port_t *buffer;
	int64_t buffer_size;
	int64_t port_count;
};

static void xpc_copy_description_level(xpc_object_t obj, struct sbuf *sbuf, int level);

static void
xpc_dictionary_destroy(struct xpc_object *dict)
{
	struct xpc_dict_head *head;
	struct xpc_dict_pair *p, *ptmp;

	head = &dict->xo_dict;

	TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
		TAILQ_REMOVE(head, p, xo_link);
		xpc_release(p->value);
		free((void *)p->key);
		free(p);
	}
}

static void
xpc_array_destroy(struct xpc_object *dict)
{
	struct xpc_object *p, *ptmp;
	struct xpc_array_head *head;

	head = &dict->xo_array;

	TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
		TAILQ_REMOVE(head, p, xo_link);
		xpc_release(p);
	}
}

void
xpc_object_destroy(struct xpc_object *xo)
{
	if (xo->xo_xpc_type == XPC_TYPE_DICTIONARY)
		xpc_dictionary_destroy(xo);

	if (xo->xo_xpc_type == XPC_TYPE_ARRAY)
		xpc_array_destroy(xo);

	if (xo->xo_xpc_type == XPC_TYPE_STRING)
		free(xo->xo_u.str);

	if (xo->xo_xpc_type == XPC_TYPE_DATA)
		free((void *)xo->xo_u.ptr);

	if (xo->xo_audit_token != NULL)
		free(xo->xo_audit_token);
}

xpc_object_t
xpc_retain(xpc_object_t obj)
{
	return os_retain(obj);
}

void
xpc_release(xpc_object_t obj)
{
	os_release(obj);
}

static const char *xpc_errors[] = {
	"No Error Found",
	"No Memory",
	"Invalid Argument",
	"No Such Process"
};


const char *
xpc_strerror(int error)
{

	if (error > EXMAX || error < 0)
		return "BAD ERROR";
	return (xpc_errors[error]);
}

static void
xpc_copy_description_level(xpc_object_t obj, struct sbuf *sbuf, int level)
{
	struct xpc_object *xo = obj;

	if (obj == NULL) {
		sbuf_printf(sbuf, "<null value>\n");
		return;
	}

	sbuf_printf(sbuf, "(%s) ", _xpc_get_type_name(obj));

	if (xo->xo_xpc_type == XPC_TYPE_DICTIONARY) {
		sbuf_printf(sbuf, "\n");
		xpc_dictionary_apply(xo, ^(const char *k, xpc_object_t v) {
			sbuf_printf(sbuf, "%*s\"%s\": ", level * 4, " ", k);
			xpc_copy_description_level(v, sbuf, level + 1);
			return ((bool)true);
		});
	} else if (xo->xo_xpc_type == XPC_TYPE_ARRAY) {
		sbuf_printf(sbuf, "\n");
		xpc_array_apply(xo, ^(size_t idx, xpc_object_t v) {
			sbuf_printf(sbuf, "%*s%ld: ", level * 4, " ", idx);
			xpc_copy_description_level(v, sbuf, level + 1);
			return ((bool)true);
		});
	} else if (xo->xo_xpc_type == XPC_TYPE_BOOL) {
		sbuf_printf(sbuf, "%s\n", xpc_bool_get_value(obj) ? "true" : "false");
	} else if (xo->xo_xpc_type == XPC_TYPE_STRING) {
		sbuf_printf(sbuf, "\"%s\"\n", xpc_string_get_string_ptr(obj));
	} else if (xo->xo_xpc_type == XPC_TYPE_INT64) {
		sbuf_printf(sbuf, "0x%llX\n", xpc_int64_get_value(obj));
	} else if (xo->xo_xpc_type == XPC_TYPE_UINT64) {
		sbuf_printf(sbuf, "0x%llX\n", xpc_uint64_get_value(obj));
	} else if (xo->xo_xpc_type == XPC_TYPE_DATE) {
		sbuf_printf(sbuf, "%llu\n", xpc_date_get_value(obj));
	} else if (xo->xo_xpc_type == XPC_TYPE_UUID) {
		uuid_t id;
		uuid_string_t uuid_str;
		memcpy(id, xpc_uuid_get_bytes(obj), sizeof(uuid_t));
		uuid_unparse_upper(id, uuid_str);
		sbuf_printf(sbuf, "%s\n", uuid_str);
		free(uuid_str);
	} else if (xo->xo_xpc_type == XPC_TYPE_ENDPOINT) {
		sbuf_printf(sbuf, "<%lld>\n", xo->xo_int);
	} else if (xo->xo_xpc_type == XPC_TYPE_NULL) {
		sbuf_printf(sbuf, "<null>\n");
	} else {
		xpc_api_misuse("Unknown XPC type");
	}
}

extern struct sbuf *sbuf_new_auto(void);

char *
xpc_copy_description(xpc_object_t obj)
{
	char *result;
	struct sbuf *sbuf;

	sbuf = sbuf_new_auto();
	xpc_copy_description_level(obj, sbuf, 0);
	sbuf_finish(sbuf);
	result = strdup(sbuf_data(sbuf));
	sbuf_delete(sbuf);

	return (result);
}

xpc_object_t
xpc_copy_entitlement_for_token(const char *key __unused, audit_token_t *token __unused)
{
	xpc_u val;

	val.b = true;
	return (_xpc_prim_create(XPC_TYPE_BOOL, val,0));
}

extern kern_return_t
mach_msg_send(mach_msg_header_t *header);

int
xpc_pipe_routine_reply(xpc_object_t xobj)
{
	xpc_assert_nonnull(xobj);

	struct xpc_object *xo;
	size_t size, msg_size;
	struct xpc_message *message;
	kern_return_t kr;
	int err;

	__block struct xpc_port_set port_set;
	port_set.buffer = calloc(16, sizeof(mach_port_t));
	port_set.buffer_size = 16;
	port_set.port_count = 0;

	xo = xobj;
	xpc_assert(xo->xo_xpc_type == XPC_TYPE_DICTIONARY, "xpc_object_t not of %s type", "dictionary");

	nvlist_t *nvlist = xpc2nv(xobj, ^(mach_port_t port) {
		int64_t port_index = port_set.port_count++;

		if (port_index > port_set.buffer_size) {
			port_set.buffer_size *= 2;
			port_set.buffer = realloc(port_set.buffer, port_set.buffer_size * sizeof(mach_port_t));
		}

		port_set.buffer[port_index] = port;
		return port_index;
	});

	if (nvlist == NULL)
		return (EINVAL);
	size = nvlist_size(nvlist);
	msg_size = __DARWIN_ALIGN(sizeof(struct xpc_message));
	if ((message = calloc(msg_size, 1)) == NULL)
		return (ENOMEM);

	void *packed = nvlist_pack_buffer(nvlist, NULL, &size);
	if (packed == NULL) {
		debugf("Could not pack XPC message for transport");
		free(message);
		nvlist_destroy(nvlist);
		return EINVAL;
	}

	message->header.msgh_size = (mach_msg_size_t)msg_size;
	message->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND) | MACH_MSGH_BITS_COMPLEX;
	message->header.msgh_remote_port = xpc_dictionary_copy_mach_send(xobj, XPC_RPORT);
	xpc_assert(message->header.msgh_remote_port != MACH_PORT_NULL, "'%s' key not found in reply", XPC_RPORT);
	message->header.msgh_local_port = MACH_PORT_NULL;
	message->id = xpc_dictionary_get_uint64(xobj, XPC_SEQID);
	xpc_assert(message->id != 0, "'%s' key not found in reply", XPC_SEQID);

	const mach_msg_ool_descriptor_t ool_data = {
		packed, // address
		size, // size
		FALSE, // deal
		MACH_MSG_VIRTUAL_COPY, // copy
		0, // pad2
		MACH_MSG_OOL_DESCRIPTOR // descriptor
	};

	const mach_msg_ool_ports_descriptor_t ool_ports = {
		port_set.buffer,
		port_set.port_count * sizeof(mach_port_t),
		FALSE,
		MACH_MSG_VIRTUAL_COPY,
		MACH_MSG_TYPE_MAKE_SEND,
		MACH_MSG_OOL_PORTS_DESCRIPTOR
	};

	message->body.msgh_descriptor_count = 2;
	message->ool_data = ool_data;
	message->ool_ports = ool_ports;

	kr = mach_msg_send(&message->header);
	if (kr != KERN_SUCCESS)
		err = (kr == KERN_INVALID_TASK) ? EPIPE : EINVAL;
	else
		err = 0;
	free(message);
	free(packed);
	free(port_set.buffer);
	nvlist_destroy(nvlist);
	return (err);
}

int
xpc_pipe_send(xpc_object_t xobj, mach_port_t dst, mach_port_t local,
    uint64_t id)
{
	struct xpc_object *xo;
	size_t size, msg_size;
	struct xpc_message *message;
	kern_return_t kr;
	int err;

	__block struct xpc_port_set port_set;
	port_set.buffer = calloc(16, sizeof(mach_port_t));
	port_set.buffer_size = 16;
	port_set.port_count = 0;

	xo = xobj;
	xpc_assert(xo->xo_xpc_type == XPC_TYPE_DICTIONARY, "xpc_object_t not of %s type", "dictionary");

	nvlist_t *nvl = xpc2nv(xobj, ^(mach_port_t port) {
		int64_t port_index = port_set.port_count++;

		if (port_index > port_set.buffer_size) {
			port_set.buffer_size *= 2;
			port_set.buffer = realloc(port_set.buffer, port_set.buffer_size * sizeof(mach_port_t));
		}

		kern_return_t kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
		xpc_assert(kr == KERN_SUCCESS, "mach_port_insert_right() failed");

		port_set.buffer[port_index] = port;
		return port_index;
	});

	size = nvlist_size(nvl);

	msg_size = __DARWIN_ALIGN(sizeof(struct xpc_message));
	if ((message = calloc(msg_size, 1)) == NULL)
		return (ENOMEM);

	void *packed = nvlist_pack_buffer(nvl, NULL, &size);
	if (packed == NULL) {
		debugf("Could not pack XPC message for transport");
		free(message);
		nvlist_destroy(nvl);
		return (EINVAL);
	}

	message->header.msgh_size = (mach_msg_size_t)msg_size;
	message->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
	    MACH_MSG_TYPE_MAKE_SEND) | MACH_MSGH_BITS_COMPLEX;
	message->header.msgh_remote_port = dst;
	message->header.msgh_local_port = local;
	message->id = id;

	const mach_msg_ool_descriptor_t ool_data = {
		packed, // address
		size, // size
		FALSE, // deallocate
		MACH_MSG_VIRTUAL_COPY, // copy
		0, // pad2
		MACH_MSG_OOL_DESCRIPTOR // descriptor
	};

	const mach_msg_ool_ports_descriptor_t ool_ports = {
		port_set.buffer,
		port_set.port_count * sizeof(mach_port_t),
		FALSE,
		MACH_MSG_VIRTUAL_COPY,
		MACH_MSG_TYPE_MOVE_SEND,
		MACH_MSG_OOL_PORTS_DESCRIPTOR
	};

	message->body.msgh_descriptor_count = 2;
	message->ool_data = ool_data;
	message->ool_ports = ool_ports;

	kr = mach_msg_send(&message->header);
	if (kr != KERN_SUCCESS) {
		debugf("mach_msg_send() failed, kr=0x%X", kr);
		err = (kr == KERN_INVALID_TASK) ? EPIPE : EINVAL;
	} else
		err = 0;
	free(packed);
	free(message);
	free(port_set.buffer);
	nvlist_destroy(nvl);
	return (err);
}

int
xpc_pipe_receive(mach_port_t local, mach_port_t *remote, xpc_object_t *result,
    uint64_t *id)
{
	struct xpc_message message;
	mach_msg_header_t *request;
	kern_return_t kr;
	mach_msg_trailer_t *tr;
	size_t data_size;
	struct xpc_object *xo;
	audit_token_t *auditp;
	xpc_u val;

	request = &message.header;
	request->msgh_size = sizeof(struct xpc_message);
	request->msgh_local_port = local;
	kr = mach_msg(request, MACH_RCV_MSG |
	    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
	    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
	    0, request->msgh_size, request->msgh_local_port,
	    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

	if (kr != 0)
		debugf("mach_msg_receive returned %d\n", kr);
	*remote = request->msgh_remote_port;
	*id = message.id;
	data_size = message.ool_data.size;
	debugf("unpacking data_size=%zu", data_size);

	nvlist_t *nv = nvlist_unpack(&message.ool_data.address, data_size);
	xo = nv2xpc(nv, ^(int64_t port_index) {
		xpc_assert(port_index <= message.ool_ports.count / sizeof(mach_port_t), "Port index greater than number of ports in buffer");
		mach_port_t *ports = message.ool_ports.address;
		return ports[port_index];
	});
	nvlist_destroy(nv);

	mig_deallocate((vm_address_t)message.ool_data.address, message.ool_data.size);
	message.ool_data.address = NULL;
	message.ool_data.size = 0;

	mig_deallocate((vm_address_t)message.ool_ports.address, message.ool_ports.count * sizeof(mach_port_t));
	message.ool_ports.address = NULL;
	message.ool_ports.count = 0;

	tr = (mach_msg_trailer_t *)(((char *)&message) + request->msgh_size);
	auditp = &((mach_msg_audit_trailer_t *)tr)->msgh_audit;

	xo->xo_audit_token = malloc(sizeof(*auditp));
	memcpy(xo->xo_audit_token, auditp, sizeof(*auditp));

	{
		struct xpc_object *xotmp;
		xpc_u val;

		val.port = request->msgh_remote_port;
		xotmp = _xpc_prim_create(XPC_TYPE_ENDPOINT, val, 0);

		xpc_dictionary_set_value_nokeycheck(xo, XPC_RPORT, xotmp);
		xpc_release(xotmp);
	}
	{
		xpc_object_t xotmp = xpc_uint64_create(message.id);
		xpc_dictionary_set_value_nokeycheck(xo, XPC_SEQID, xotmp);
		xpc_release(xotmp);
	}

	xo->xo_flags |= _XPC_FROM_WIRE;
	*result = xo;
	return (0);
}

int
xpc_pipe_try_receive(mach_port_t portset, xpc_object_t *requestobj, mach_port_t *rcvport,
	boolean_t (*demux)(mach_msg_header_t *, mach_msg_header_t *), mach_msg_size_t msgsize,
	int flags __unused)
{
	struct xpc_message *message;
	mach_msg_header_t *request;
	kern_return_t kr;
	mach_msg_header_t *response;
	mach_msg_trailer_t *tr;
	int data_size;
	struct xpc_object *xo;
	audit_token_t *auditp;
	mach_msg_size_t receive_size = msgsize;
	mach_msg_size_t response_size = msgsize;

	if (receive_size < sizeof(struct xpc_message)) {
		receive_size = sizeof(struct xpc_message);
	}
	if (response_size < sizeof(struct xpc_message)) {
		response_size = sizeof(struct xpc_message);
	}
	receive_size += sizeof(mach_msg_audit_trailer_t);

	message = calloc(1, receive_size);
	response = calloc(1, response_size);
	if (message == NULL || response == NULL) {
		free(message);
		free(response);
		return ENOMEM;
	}

	request = &message->header;
	request->msgh_size = receive_size;
	request->msgh_local_port = portset;
	kr = mach_msg(request, MACH_RCV_MSG |
	    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
	    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
	    0, request->msgh_size, request->msgh_local_port,
	    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

	if (kr != 0) {
		int fd = open("/dev/console", O_WRONLY | O_NOCTTY);
		if (fd >= 0) {
			dprintf(fd, "xpc_pipe_try_receive: kr=0x%x bits=0x%x size=%u local=0x%x remote=0x%x id=%u\n",
					kr, request->msgh_bits, request->msgh_size,
					request->msgh_local_port, request->msgh_remote_port, request->msgh_id);
			close(fd);
		}
		debugf("mach_msg_receive returned %d\n", kr);
		free(message);
		free(response);
		return kr;
	}
	*rcvport = request->msgh_remote_port;
	if (demux(request, response)) {
		kr = mach_msg_send(response);
		if (kr != 0 && request->msgh_id == 407) {
			int fd = open("/dev/console", O_WRONLY | O_NOCTTY);
			if (fd >= 0) {
				dprintf(fd, "xpc_pipe_try_receive: reply kr=0x%x id=%u reply_id=%u remote=0x%x ret=0x%x\n",
						kr, request->msgh_id, response->msgh_id,
						response->msgh_remote_port, ((mig_reply_error_t *)response)->RetCode);
				close(fd);
			}
		}
		/*  can't do anything with the return code
		* just tell the caller this has been handled
		*/
		free(message);
		free(response);
		return (TRUE);
	}
	debugf("demux returned false\n");
	data_size = message->ool_data.size;
	debugf("unpacking data_size=%d", data_size);

	nvlist_t *nvlist = nvlist_unpack(&message->ool_data.address, data_size);
	xo = nv2xpc(nvlist, ^(int64_t port_index) {
		xpc_assert(port_index <= message->ool_ports.count / sizeof(mach_port_t), "Port index greater than number of ports in buffer");
		mach_port_t *ports = message->ool_ports.address;
		return ports[port_index];
	});
	nvlist_destroy(nvlist);

	mig_deallocate((vm_address_t)message->ool_data.address, message->ool_data.size);
	message->ool_data.address = NULL;
	message->ool_data.size = 0;

	mig_deallocate((vm_address_t)message->ool_ports.address, message->ool_ports.count * sizeof(mach_port_t));
	message->ool_ports.address = NULL;
	message->ool_ports.count = 0;

	/* is padding for alignment enforced in the kernel?*/
	tr = (mach_msg_trailer_t *)(((char *)message) + request->msgh_size);
	auditp = &((mach_msg_audit_trailer_t *)tr)->msgh_audit;

	xo->xo_audit_token = malloc(sizeof(*auditp));
	memcpy(xo->xo_audit_token, auditp, sizeof(*auditp));

	xpc_dictionary_set_mach_send(xo, XPC_RPORT, request->msgh_remote_port);
	xpc_dictionary_set_uint64(xo, XPC_SEQID, message->id);
	xo->xo_flags |= _XPC_FROM_WIRE;
	*requestobj = xo;
	free(message);
	free(response);
	return (0);
}


int
xpc_call_wakeup(mach_port_t rport, int retcode)
{
	mig_reply_error_t msg;
	int err;
	kern_return_t kr;

	msg.Head.msgh_remote_port = rport;
	msg.RetCode = retcode;
	kr = mach_msg_send(&msg.Head);
	if (kr != KERN_SUCCESS)
		err = (kr == KERN_INVALID_TASK) ? EPIPE : EINVAL;
	else
		err = 0;

	return (err);
}
