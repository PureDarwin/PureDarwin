/*
 * Copyright (c) 2003-2010 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <asl.h>
#include "notify.h"
#include "notifyd.h"
#include "service.h"
#include "pathwatch.h"
#include "notify_internal.h"

#define NOTIFY_PATH_SERVICE "path:"
#define NOTIFY_PATH_SERVICE_LEN 5

/* Libinfo global */
extern uint32_t gL1CacheEnabled;

static uint32_t
service_type(const char *name)
{
	uint32_t len;

	len = SERVICE_PREFIX_LEN;
	if (strncmp(name, SERVICE_PREFIX, len)) return SERVICE_TYPE_NONE;
	else if (!strncmp(name + len, NOTIFY_PATH_SERVICE, NOTIFY_PATH_SERVICE_LEN)) return SERVICE_TYPE_PATH_PRIVATE;
	
	return SERVICE_TYPE_NONE;
}

/*
 * Request notifications for changes on a filesystem path.
 * This creates a new pathwatch node and sets it to post notifications for
 * the specified name.
 *
 * If the notify name already has a pathwatch node for this path, this routine
 * does nothing and allows the client to piggypack on the existing path watcher.
 *
 * Note that this routine is only called for path monitoring as directed by
 * a "monitor" command in /etc/notify.conf, so only an admin can set up a path
 * that gets public notifications.  A client being serviced by the server-side
 * routines in notify_proc.c will only be able to register for a private 
 * (per-client) notification for a path.  This prevents a client from 
 * piggybacking on another client's notifications, and thus prevents the client
 * from getting notifications for a path to which they don't have access. 
 */
int
service_open_path(const char *name, const char *path, uid_t uid, gid_t gid)
{
	name_info_t *n;
	path_node_t *node;

	call_statistics.service_path++;

	if (path == NULL) return NOTIFY_STATUS_INVALID_REQUEST;

	n = _nc_table_find(&global.notify_state.name_table, name);
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;

	{
		audit_token_t audit;
		memset(&audit, 0, sizeof(audit_token_t));
		node = path_node_create(path, audit, true, PATH_NODE_ALL);
	}

	if (node == NULL) return NOTIFY_STATUS_PATH_NODE_CREATE_FAILED;
	
	node->contextp = strdup(name);

	dispatch_source_set_event_handler(node->src, ^{
		daemon_post((const char *)node->contextp, uid, gid);
	});
	
	dispatch_activate(node->src);

	return NOTIFY_STATUS_OK;
}

static uint16_t service_info_add(void *info)
{
	assert(global.service_info_count != UINT16_MAX);

	for(int i = 0; i < global.service_info_count; i++)
	{
		if(global.service_info_list[i] == NULL){
			global.service_info_list[i] = info;
			return i + 1;
		}
	}

	if(global.service_info_count == 0){
		global.service_info_count = 1;
		global.service_info_list = malloc(sizeof(void *));
	} else {
		global.service_info_count++;
		global.service_info_list = realloc(global.service_info_list, global.service_info_count * sizeof(void *));
	}

	global.service_info_list[global.service_info_count - 1] = info;

	return global.service_info_count;
}

void *service_info_get(uint16_t index)
{
	if(index == 0)
	{
		return NULL;
	}

	return global.service_info_list[index - 1];
}


static void *service_info_remove(uint16_t index)
{
	if(index == 0)
	{
		return NULL;
	}

	void *ret = global.service_info_list[index - 1];

	global.service_info_list[index - 1] = NULL;

	return ret;

}


/*
 * The private (per-client) path watch service.
 * Must be callled on global.workloop if it is initialized
 */
int
service_open_path_private(const char *name, client_t *c, const char *path, audit_token_t audit, uint32_t flags)
{
	name_info_t *n;
	svc_info_t *info;
	path_node_t *node;

	call_statistics.service_path++;

	if (path == NULL) return NOTIFY_STATUS_INVALID_REQUEST;
	
	n = _nc_table_find(&global.notify_state.name_table, name);
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (c == NULL) return NOTIFY_STATUS_NULL_INPUT;

	if (c->service_index != 0)
	{
		/* a client may only have one service */
		info = (svc_info_t *)service_info_get(c->service_index);
		if (info->type != SERVICE_TYPE_PATH_PRIVATE) return NOTIFY_STATUS_INVALID_REQUEST;
		
		/* the client must be asking for the same path that is being monitored */
		node = (path_node_t *)info->private;
		if (strcmp(path, node->path)) return NOTIFY_STATUS_INVALID_REQUEST;
		
		/* the client is already getting notifications for this path */
		return NOTIFY_STATUS_OK;
	}

	if (flags == 0) flags = PATH_NODE_ALL;

	node = path_node_create(path, audit, false, flags);
	if (node == NULL) return NOTIFY_STATUS_PATH_NODE_CREATE_FAILED;
	
	node->context64 = c->cid.hash_key;
	
	info = (svc_info_t *)calloc(1, sizeof(svc_info_t));
	assert(info != NULL);
	
	info->type = SERVICE_TYPE_PATH_PRIVATE;
	info->private = node;
	c->service_index = service_info_add(info);
	
	dispatch_source_set_event_handler(node->src, ^{
		daemon_post_client(node->context64);
	});
	
	dispatch_activate(node->src);
	
	return NOTIFY_STATUS_OK;
}

/* called from server-side routines in notify_proc - services are private to the client */
int
service_open(const char *name, client_t *client, audit_token_t audit)
{
	uint32_t t, flags;
    char *p, *q;
 
	t = service_type(name);

	switch (t)
	{
		case SERVICE_TYPE_NONE:
		{
			return NOTIFY_STATUS_OK;
		}
		case SERVICE_TYPE_PATH_PRIVATE:
		{
			p = strchr(name, ':');
			if (p != NULL) p++;

			flags = 0;

			q = strchr(p, ':');
			if (q != NULL) 
			{
				flags = (uint32_t)strtol(p, NULL, 0);
				p = q + 1;
			}

			return service_open_path_private(name, client, p, audit, flags);
		}
		default: 
		{
			return NOTIFY_STATUS_INVALID_REQUEST;
		}
	}

	return NOTIFY_STATUS_INVALID_REQUEST;
}

void
service_close(uint16_t service_index)
{
	if (service_index == 0) return;

	svc_info_t *info = service_info_remove(service_index);

	switch (info->type)
	{
		case SERVICE_TYPE_PATH_PUBLIC:
		case SERVICE_TYPE_PATH_PRIVATE:
		{
			path_node_close((path_node_t *)info->private);
			break;
		}
		default:
		{
		}
	}

	free(info);
}
