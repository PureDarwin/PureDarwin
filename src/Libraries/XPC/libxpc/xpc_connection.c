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

#include <errno.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <xpc/xpc.h>
#include <stdatomic.h>
#include <bsm/libbsm.h>
#include <Block.h>
#include "vproc.h"
#include "xpc_internal.h"

#define XPC_CONNECTION_NEXT_ID(conn) atomic_fetch_add(&conn->xc_last_id, 1)

static void xpc_connection_recv_message(void *);
static void xpc_send(xpc_connection_t xconn, xpc_object_t message, uint64_t id);
static void xpc_connection_deliver_invalid(struct xpc_connection *conn);

OS_OBJECT_OBJC_CLASS_DECL(xpc_connection);

xpc_connection_t
xpc_connection_create(const char *name, dispatch_queue_t targetq)
{
	kern_return_t kr;
	char *qname;
	struct xpc_connection *conn;

	conn = _os_object_alloc(&OS_xpc_connection_class, sizeof(struct xpc_connection) - sizeof(struct xpc_object_header));
	if (conn == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	memset(conn, 0, sizeof(struct xpc_connection));
	conn->xc_xpc_type = XPC_TYPE_CONNECTION;
	conn->xc_name = name != NULL ? strdup(name) : NULL;
	conn->xc_last_id = 1;
	TAILQ_INIT(&conn->xc_peers);
	TAILQ_INIT(&conn->xc_pending);

	/* Create send queue */
	asprintf(&qname, "com.ixsystems.xpc.connection.sendq.%p", conn);
	conn->xc_send_queue = dispatch_queue_create(qname, NULL);
	free(qname);

	/* Create recv queue */
	asprintf(&qname, "com.ixsystems.xpc.connection.recvq.%p", conn);
	conn->xc_recv_queue = dispatch_queue_create(qname, NULL);
	free(qname);

	/* Create target queue */
	conn->xc_target_queue = targetq ? targetq : dispatch_get_main_queue();

	/* Receive queue is initially suspended */
	dispatch_suspend(conn->xc_recv_queue);

	/* Create local port */
	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
	    &conn->xc_local_port);
	if (kr != KERN_SUCCESS) {
		errno = EPERM;
		xpc_release(conn);
		return (NULL);
	}

	kr = mach_port_insert_right(mach_task_self(), conn->xc_local_port,
	    conn->xc_local_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		errno = EPERM;
		xpc_release(conn);
		return (NULL);
	}

	return (conn);
}

xpc_connection_t
xpc_connection_create_mach_service(const char *name, dispatch_queue_t targetq,
    uint64_t flags)
{
	kern_return_t kr;
	struct xpc_connection *conn;

	conn = xpc_connection_create(name, targetq);
	if (conn == NULL)
		return (NULL);

	conn->xc_flags = flags;

	if (flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
		kr = bootstrap_check_in(bootstrap_port, name,
		    &conn->xc_local_port);
		if (kr != KERN_SUCCESS) {
			errno = EBUSY;
			xpc_release(conn);
			return (NULL);
		}

		return (conn);
	}

	if (!strcmp(name, "bootstrap")) {
		conn->xc_remote_port = bootstrap_port;
		return (conn);
	}

	/* Look up named mach service */
	kr = bootstrap_look_up(bootstrap_port, name, &conn->xc_remote_port);
	if (kr != KERN_SUCCESS) {
		errno = ENOENT;
		xpc_release(conn);
		return (NULL);
	}

	return (conn);
}

xpc_connection_t
xpc_connection_create_from_endpoint(xpc_endpoint_t endpoint)
{
	struct xpc_connection *conn;
	struct xpc_object *xo = (struct xpc_object *)endpoint;

	xpc_assert_nonnull(xo);
	xpc_assert_type(xo, XPC_TYPE_ENDPOINT);

	conn = xpc_connection_create(NULL, NULL);
	if (conn == NULL)
		return (NULL);

	conn->xc_remote_port = xo->xo_port;
	return (conn);
}

void
xpc_connection_set_target_queue(xpc_connection_t xconn,
    dispatch_queue_t targetq)
{
	struct xpc_connection *conn;

	debugf("connection=%p", xconn);
	conn = xconn;
	conn->xc_target_queue = targetq;
}

void
xpc_connection_set_event_handler(xpc_connection_t xconn,
    xpc_handler_t handler)
{
	struct xpc_connection *conn;

	debugf("connection=%p", xconn);
	conn = xconn;
	if (conn->xc_handler != NULL)
		Block_release(conn->xc_handler);
	conn->xc_handler = Block_copy(handler);
}

void
xpc_connection_suspend(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	conn->xc_suspend_count++;
	if (conn->xc_recv_source == NULL)
		return;
	dispatch_suspend(conn->xc_recv_source);
}

void
xpc_connection_resume(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	debugf("connection=%p", xconn);
	conn = xconn;
	if (conn->xc_cancelled)
		return;

	/* Create dispatch source for top-level connection */
	if (conn->xc_parent == NULL) {
		conn->xc_recv_source = dispatch_source_create(
		    DISPATCH_SOURCE_TYPE_MACH_RECV, conn->xc_local_port, 0,
		    conn->xc_recv_queue);
		dispatch_set_context(conn->xc_recv_source, conn);
		dispatch_source_set_event_handler_f(conn->xc_recv_source,
		    xpc_connection_recv_message);
		dispatch_resume(conn->xc_recv_source);
	}

	if (!conn->xc_started) {
		conn->xc_started = true;
		dispatch_resume(conn->xc_recv_queue);
	} else if (conn->xc_suspend_count > 0) {
		conn->xc_suspend_count--;
		dispatch_resume(conn->xc_recv_queue);
	}
}

void
xpc_connection_send_message(xpc_connection_t xconn,
    xpc_object_t message)
{
	struct xpc_connection *conn;
	uint64_t id;

	conn = xconn;
	id = xpc_dictionary_get_uint64(message, XPC_SEQID);

	if (id == 0)
		id = XPC_CONNECTION_NEXT_ID(conn);

	dispatch_async(conn->xc_send_queue, ^{
		xpc_send(conn, message, id);
	});
}

void
xpc_connection_send_message_with_reply(xpc_connection_t xconn,
    xpc_object_t message, dispatch_queue_t targetq, xpc_handler_t handler)
{
	struct xpc_connection *conn;
	struct xpc_pending_call *call;

	conn = xconn;
	call = malloc(sizeof(struct xpc_pending_call));
	call->xp_id = XPC_CONNECTION_NEXT_ID(conn);
	call->xp_handler = Block_copy(handler);
	call->xp_queue = targetq ? targetq : conn->xc_target_queue;
	TAILQ_INSERT_TAIL(&conn->xc_pending, call, xp_link);

	dispatch_async(conn->xc_send_queue, ^{
		xpc_send(conn, message, call->xp_id);
	});

}

xpc_object_t
xpc_connection_send_message_with_reply_sync(xpc_connection_t conn,
    xpc_object_t message)
{
	__block xpc_object_t result;
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);

	xpc_connection_send_message_with_reply(conn, message, NULL,
	    ^(xpc_object_t o) {
		result = o;
		dispatch_semaphore_signal(sem);
	});

	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
	return (result);
}

void
xpc_connection_send_barrier(xpc_connection_t xconn, dispatch_block_t barrier)
{
	struct xpc_connection *conn;

	conn = xconn;
	if (conn->xc_cancelled) {
		barrier();
		return;
	}
	dispatch_sync(conn->xc_send_queue, barrier);
}

void
xpc_connection_cancel(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	if (conn->xc_cancelled)
		return;

	conn->xc_cancelled = true;
	if (conn->xc_recv_source != NULL)
		dispatch_source_cancel(conn->xc_recv_source);

	xpc_connection_deliver_invalid(conn);

}

const char *
xpc_connection_get_name(xpc_connection_t connection)
{
	struct xpc_connection *conn;

	conn = connection;
	return (conn->xc_name);
}

uid_t
xpc_connection_get_euid(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	return (conn->xc_remote_euid);
}

gid_t
xpc_connection_get_egid(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	return (conn->xc_remote_guid);
}

pid_t
xpc_connection_get_pid(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	return (conn->xc_remote_pid);
}

au_asid_t
xpc_connection_get_asid(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	return (conn->xc_remote_asid);
}

void
xpc_connection_set_context(xpc_connection_t xconn, void *ctx)
{
	struct xpc_connection *conn;

	conn = xconn;
	conn->xc_context = ctx;
}

void *
xpc_connection_get_context(xpc_connection_t xconn)
{
	struct xpc_connection *conn;

	conn = xconn;
	return (conn->xc_context);
}

void
xpc_connection_set_finalizer_f(xpc_connection_t connection,
    xpc_finalizer_t finalizer)
{
	struct xpc_connection *conn;

	conn = connection;
	conn->xc_finalizer = finalizer;
}

xpc_endpoint_t
xpc_endpoint_create(xpc_connection_t connection)
{
	struct xpc_connection *conn;
	xpc_u val;

	conn = connection;
	val.port = conn->xc_local_port;
	return (xpc_endpoint_t)_xpc_prim_create(XPC_TYPE_ENDPOINT, val, 0);
}

void
xpc_main(xpc_connection_handler_t handler)
{
	(void)handler;

	dispatch_main();
}

void
xpc_transaction_begin(void)
{
	vproc_transaction_begin(NULL);
}

void
xpc_transaction_end(void)
{
	vproc_transaction_end(NULL, NULL);
}

static void
xpc_send(xpc_connection_t xconn, xpc_object_t message, uint64_t id)
{
	struct xpc_connection *conn;
	int error_code;

	debugf("connection=%p, message=%p, id=%llu", xconn, message, id);

	conn = xconn;
	if (conn->xc_cancelled)
		return;

	error_code = xpc_pipe_send(message, conn->xc_remote_port,
	    conn->xc_local_port, id);

	if (error_code != 0)
		debugf("send failed, errno=%s", strerror(error_code));
}

static void
xpc_connection_set_credentials(struct xpc_connection *conn, audit_token_t *tok)
{
	if (tok == NULL)
		return;

	// Yuck. We really shouldn't be taking this kind of dependency
	// on the internal guts of the audit_token_t. However, that being
	// said, there is no other way to extract the salient data from the
	// token. I cannot link to libbsm and use the official APIs, because
	// libbsm is a higher-level API than libxpc. This is a layering violation.
	// Nor do I consider it acceptable to dlopen() libbsm at runtime, as that
	// would be practically the same thing (xpc_connection_set_credentials()
	// is frequently called AFAICT).

	// This code came from openbsm/libbsm/bsm_wrappers.c in OpenBSM-21.
	conn->xc_remote_euid = tok->val[1];
	conn->xc_remote_guid = tok->val[2];
	conn->xc_remote_pid = tok->val[5];
	conn->xc_remote_asid = tok->val[6];
}

static void
xpc_connection_recv_message(void *context)
{
	struct xpc_pending_call *call;
	struct xpc_connection *conn, *peer;
	xpc_object_t result;
	mach_port_t remote;
	kern_return_t kr;
	uint64_t id;

	debugf("connection=%p", context);

	conn = context;
	if (conn->xc_cancelled)
		return;

	kr = xpc_pipe_receive(conn->xc_local_port, &remote, &result, &id);
	if (kr != KERN_SUCCESS)
		return;

	debugf("message=%p, id=%llu, remote=<%d>", result, id, remote);

	if (conn->xc_flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
		TAILQ_FOREACH(peer, &conn->xc_peers, xc_link) {
			if (remote == peer->xc_remote_port) {
				((struct xpc_object *)result)->xo_remote_connection = peer;
				dispatch_async(peer->xc_target_queue, ^{
					peer->xc_handler(result);
					xpc_release(result);
				});
				return;
			}
		}

		debugf("new peer on port <%u>", remote);

		/* New peer */
		peer = xpc_connection_create(NULL, NULL);
		peer->xc_parent = conn;
		peer->xc_remote_port = remote;
		xpc_connection_set_credentials(peer,
		    ((struct xpc_object *)result)->xo_audit_token);

		TAILQ_INSERT_TAIL(&conn->xc_peers, peer, xc_link);

		dispatch_async(conn->xc_target_queue, ^{
			conn->xc_handler(peer);
		});

		((struct xpc_object *)result)->xo_remote_connection = peer;

		dispatch_async(peer->xc_target_queue, ^{
			peer->xc_handler(result);
			xpc_release(result);
		});

	} else {
		xpc_connection_set_credentials(conn,
		    ((struct xpc_object *)result)->xo_audit_token);
		((struct xpc_object *)result)->xo_remote_connection = conn;

		TAILQ_FOREACH(call, &conn->xc_pending, xp_link) {
			if (call->xp_id == id) {
				dispatch_async(call->xp_queue, ^{
					call->xp_handler(result);
					TAILQ_REMOVE(&conn->xc_pending, call,
					    xp_link);
					Block_release(call->xp_handler);
					free(call);
					xpc_release(result);
				});
				return;
			}
		}

		if (conn->xc_handler) {
			dispatch_async(conn->xc_target_queue, ^{
			    conn->xc_handler(result);
			    xpc_release(result);
			});
		} else {
			xpc_release(result);
		}
	}
}

static void
xpc_connection_deliver_invalid(struct xpc_connection *conn)
{
	struct xpc_pending_call *call, *tmp;
	xpc_handler_t handler;

	TAILQ_FOREACH_SAFE(call, &conn->xc_pending, xp_link, tmp) {
		TAILQ_REMOVE(&conn->xc_pending, call, xp_link);
		handler = call->xp_handler;
		dispatch_async(call->xp_queue ? call->xp_queue : conn->xc_target_queue, ^{
			handler(XPC_ERROR_CONNECTION_INVALID);
			Block_release(handler);
		});
		free(call);
	}

	if (conn->xc_handler != NULL) {
		handler = Block_copy(conn->xc_handler);
		dispatch_async(conn->xc_target_queue, ^{
			handler(XPC_ERROR_CONNECTION_INVALID);
			Block_release(handler);
		});
	}
}

__private_extern__ void
xpc_connection_destroy(struct xpc_connection *conn)
{
	struct xpc_pending_call *call, *call_tmp;
	struct xpc_connection *peer, *peer_tmp;

	if (conn->xc_finalizer != NULL)
		conn->xc_finalizer(conn->xc_context);

	TAILQ_FOREACH_SAFE(call, &conn->xc_pending, xp_link, call_tmp) {
		TAILQ_REMOVE(&conn->xc_pending, call, xp_link);
		if (call->xp_handler != NULL)
			Block_release(call->xp_handler);
		free(call);
	}

	TAILQ_FOREACH_SAFE(peer, &conn->xc_peers, xc_link, peer_tmp) {
		TAILQ_REMOVE(&conn->xc_peers, peer, xc_link);
		xpc_release(peer);
	}

	if (conn->xc_handler != NULL)
		Block_release(conn->xc_handler);
	if (conn->xc_local_port != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), conn->xc_local_port);
	if (conn->xc_remote_port != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), conn->xc_remote_port);
	free((void *)conn->xc_name);
}

void
xpc_create_from_plist(void)
{
	xpc_api_misuse("%s: Function unimplemented", __FUNCTION__);
}
