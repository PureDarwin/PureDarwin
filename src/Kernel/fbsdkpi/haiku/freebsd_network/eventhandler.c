/*
 * Copyright 2009, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Michael Weirauch, dev@m-phasis.de
 */


#include <compat/sys/types.h>

#include <compat/sys/eventhandler.h>


eventhandler_tag
eventhandler_register(struct eventhandler_list *list, 
	    const char *name, void *func, void *arg, int priority)
{
	return NULL;
};


void
eventhandler_deregister(struct eventhandler_list *list,
	    eventhandler_tag tag)
{
	//
};


struct eventhandler_list *
eventhandler_find_list(const char *name)
{
	return NULL;
};


void
eventhandler_prune_list(struct eventhandler_list *list)
{
	//
};
