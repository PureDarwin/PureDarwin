/*
 * xpc_pipe.c - synchronous XPC request/reply over a Mach send right.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <sys/types.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <xpc/launchd.h>

#include "xpc_internal.h"

OS_OBJECT_OBJC_CLASS_DECL(xpc_pipe);

struct xpc_pipe {
	struct xpc_object_header header;
	_Atomic(mach_port_t) xp_port;
	uint64_t xp_flags;
};

static _Atomic(uint64_t) xpc_pipe_next_id = 1;

/* Takes ownership of one send right to port. */
xpc_pipe_t
xpc_pipe_create_from_port(mach_port_t port, uint64_t flags)
{
	struct xpc_pipe *pipe;

	if (!MACH_PORT_VALID(port)) {
		return NULL;
	}
	pipe = (struct xpc_pipe *)_pd_xpc_object_alloc(&OS_xpc_pipe_class,
	    sizeof(*pipe) - sizeof(struct xpc_object_header));
	if (pipe == NULL) {
		return NULL;
	}
	atomic_store(&pipe->xp_port, port);
	pipe->xp_flags = flags;
	return (xpc_pipe_t)pipe;
}

xpc_pipe_t
xpc_pipe_create(const char *name, uint64_t flags)
{
	mach_port_t port = MACH_PORT_NULL;
	xpc_pipe_t pipe;

	if (name == NULL || bootstrap_look_up(bootstrap_port, name, &port) != KERN_SUCCESS) {
		return NULL;
	}
	pipe = xpc_pipe_create_from_port(port, flags);
	if (pipe == NULL) {
		mach_port_deallocate(mach_task_self(), port);
	}
	return pipe;
}

void
xpc_pipe_invalidate(xpc_pipe_t xpipe)
{
	struct xpc_pipe *pipe = (struct xpc_pipe *)xpipe;
	mach_port_t port;

	if (pipe == NULL) {
		return;
	}
	port = atomic_exchange(&pipe->xp_port, MACH_PORT_NULL);
	if (MACH_PORT_VALID(port)) {
		mach_port_deallocate(mach_task_self(), port);
	}
}

void
xpc_pipe_destroy(struct xpc_pipe *pipe)
{
	xpc_pipe_invalidate((xpc_pipe_t)pipe);
}

int
xpc_pipe_simpleroutine(xpc_pipe_t xpipe, xpc_object_t message)
{
	struct xpc_pipe *pipe = (struct xpc_pipe *)xpipe;
	mach_port_t port;

	if (pipe == NULL || message == NULL || xpc_get_type(message) != XPC_TYPE_DICTIONARY) {
		return EINVAL;
	}
	port = atomic_load(&pipe->xp_port);
	if (!MACH_PORT_VALID(port)) {
		return EPIPE;
	}
	return xpc_pipe_send_local(message, port, MACH_PORT_NULL, MACH_MSG_TYPE_MAKE_SEND,
	    atomic_fetch_add(&xpc_pipe_next_id, 1));
}

/*
 * The reply comes back on a fresh port the server holds a send-once right to,
 * so a server that dies without replying wakes us with a send-once
 * notification instead of leaving us blocked.
 */
int
xpc_pipe_routine(xpc_pipe_t xpipe, xpc_object_t request, xpc_object_t *reply)
{
	struct xpc_pipe *pipe = (struct xpc_pipe *)xpipe;
	mach_port_t port, reply_port, remote;
	xpc_object_t result = NULL;
	uint64_t id, reply_id = 0;
	int error;

	if (reply != NULL) {
		*reply = NULL;
	}
	if (pipe == NULL || request == NULL || reply == NULL ||
	    xpc_get_type(request) != XPC_TYPE_DICTIONARY) {
		return EINVAL;
	}
	port = atomic_load(&pipe->xp_port);
	if (!MACH_PORT_VALID(port)) {
		return EPIPE;
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply_port) != KERN_SUCCESS) {
		return ENOMEM;
	}

	id = atomic_fetch_add(&xpc_pipe_next_id, 1);
	error = xpc_pipe_send_local(request, port, reply_port, MACH_MSG_TYPE_MAKE_SEND_ONCE, id);
	if (error == 0) {
		error = xpc_pipe_receive(reply_port, &remote, &result, &reply_id);
	}
	if (error == 0 && reply_id != id) {
		xpc_release(result);
		result = NULL;
		error = EINVAL;
	}
	mach_port_mod_refs(mach_task_self(), reply_port, MACH_PORT_RIGHT_RECEIVE, -1);

	if (error == 0) {
		*reply = result;
	}
	return error;
}
