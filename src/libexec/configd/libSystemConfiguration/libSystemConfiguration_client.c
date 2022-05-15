/*
 * Copyright (c) 2012, 2013, 2015, 2016, 2018, 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <TargetConditionals.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <os/log.h>
#include <vproc.h>
#include <vproc_priv.h>
#include <xpc/xpc.h>

#include "libSystemConfiguration_client.h"
#include "libSystemConfiguration_internal.h"


#pragma mark -
#pragma mark libSC fork handlers


static boolean_t _available	= TRUE;

// These functions are registered with libSystem to
// handle pthread_atfork callbacks.

void
_libSC_info_fork_prepare()
{
	return;
}

void
_libSC_info_fork_parent()
{
	return;
}

void
_libSC_info_fork_child()
{
	if (_dispatch_is_fork_of_multithreaded_parent()) {
		_available = FALSE;
	}

	return;
}


#pragma mark -
#pragma mark Support functions


static void
log_xpc_object(const char *msg, xpc_object_t obj)
{
	char	*desc;

	desc = xpc_copy_description(obj);
	os_log(OS_LOG_DEFAULT, "%s = %s", msg, desc);
	free(desc);
}


__private_extern__
_Bool
libSC_info_available()
{
	return _available;
}


static void
libSC_info_client_dealloc(libSC_info_client_t *client)
{
	free(client->service_description);
	free(client->service_name);
	free(client);
	return;
}


__private_extern__
libSC_info_client_t *
libSC_info_client_create(dispatch_queue_t	q,
			 const char		*service_name,
			 const char		*service_description)
{
	xpc_connection_t	c;
	libSC_info_client_t	*client;
#if	!TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST
	const uint64_t		flags	=	XPC_CONNECTION_MACH_SERVICE_PRIVILEGED;
#else	// !TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST
	const uint64_t		flags	=	0;
#endif	// !TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST

	if (!_available) {
		return NULL;
	}

	client = malloc(sizeof(libSC_info_client_t));
	client->active = TRUE;
	client->service_description = strdup(service_description);
	client->service_name = strdup(service_name);

	c = xpc_connection_create_mach_service(service_name, q, flags);

	xpc_connection_set_event_handler(c, ^(xpc_object_t xobj) {
		xpc_type_t	type;

		type = xpc_get_type(xobj);
		if (type == XPC_TYPE_DICTIONARY) {
			os_log(OS_LOG_DEFAULT, "%s: unexpected message", client->service_name);
			log_xpc_object("  dict = ", xobj);
		} else if (type == XPC_TYPE_ERROR) {
			if (xobj == XPC_ERROR_CONNECTION_INVALID) {
				os_log(OS_LOG_DEFAULT, "%s: server not available", client->service_name);
				client->active = FALSE;
			} else if (xobj == XPC_ERROR_CONNECTION_INTERRUPTED) {
				os_log_debug(OS_LOG_DEFAULT, "%s: server failed", client->service_name);
			} else {
				const char	*desc;

				desc = xpc_dictionary_get_string(xobj, XPC_ERROR_KEY_DESCRIPTION);
				os_log_debug(OS_LOG_DEFAULT,
					     "%s: connection error: %d : %s",
					     client->service_name,
					     xpc_connection_get_pid(c),
					     desc);
			}
		} else {
			os_log(OS_LOG_DEFAULT,
			       "%s: unknown event type : %p",
			       client->service_name,
			       type);
		}
	});

	client->connection = c;

	xpc_connection_set_context(c, client);
	xpc_connection_set_finalizer_f(c, (xpc_finalizer_t)libSC_info_client_dealloc);

	xpc_connection_resume(c);

	return client;
}


__private_extern__
void
libSC_info_client_release(libSC_info_client_t *client)
{
	xpc_release(client->connection);
	return;
}


__private_extern__
xpc_object_t
libSC_send_message_with_reply_sync(libSC_info_client_t	*client,
				   xpc_object_t		message)
{
	xpc_object_t	reply;

	while (TRUE) {
		// send request to the DNS configuration server
		reply = xpc_connection_send_message_with_reply_sync(client->connection, message);
		if (reply != NULL) {
			xpc_type_t      type;

			type = xpc_get_type(reply);
			if (type == XPC_TYPE_DICTIONARY) {
				// reply available
				break;
			}

			if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INTERRUPTED)) {
				os_log_debug(OS_LOG_DEFAULT,
					     "%s server failure, retrying",
					     client->service_description);
				// retry request
				xpc_release(reply);
				continue;
			}

			if ((type == XPC_TYPE_ERROR) && (reply == XPC_ERROR_CONNECTION_INVALID)) {
				os_log(OS_LOG_DEFAULT,
				       "%s server not available",
				       client->service_description);
				client->active = FALSE;
			} else {
				os_log(OS_LOG_DEFAULT,
				       "%s xpc_connection_send_message_with_reply_sync() with unexpected reply",
				       client->service_description);
				log_xpc_object("  reply", reply);
			}

			xpc_release(reply);
			reply = NULL;
			break;
		}
	}

	return reply;
}
