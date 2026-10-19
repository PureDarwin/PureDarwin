/*
 * Copyright 2004-2010, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DRIVERS_SELECT_H
#define _DRIVERS_SELECT_H


#include <SupportDefs.h>


struct selectsync;
typedef struct selectsync selectsync;

enum select_events {
	B_SELECT_READ = 1,			// standard select() support
	B_SELECT_WRITE,
	B_SELECT_ERROR,

	B_SELECT_PRI_READ,			// additional poll() support
	B_SELECT_PRI_WRITE,

	B_SELECT_HIGH_PRI_READ,
	B_SELECT_HIGH_PRI_WRITE,

	B_SELECT_DISCONNECTED
};


#ifdef __cplusplus
extern "C" {
#endif

extern status_t notify_select_event(struct selectsync *sync, uint8 event);

#ifdef __cplusplus
}
#endif

#endif	/* _DRIVERS_SELECT_H */
