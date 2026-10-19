/*
 * Copyright 2002-2003, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FS_QUERY_H
#define _FS_QUERY_H


#include <OS.h>
#include <dirent.h>


/* Flags for fs_open_[live_]query() */
enum {
	B_LIVE_QUERY		= (1 << 0),
	B_QUERY_NON_INDEXED	= (1 << 1),
	B_QUERY_WATCH_ALL	= (1 << 2),
};


#ifdef  __cplusplus
extern "C" {
#endif

extern DIR		*fs_open_query(dev_t device, const char *query, uint32 flags);
extern DIR		*fs_open_live_query(dev_t device, const char *query,
					uint32 flags, port_id port, int32 token);
extern int		fs_close_query(DIR *d);
extern struct dirent *fs_read_query(DIR *d);

extern status_t	get_path_for_dirent(struct dirent *dent, char *buf,
					size_t len);

#ifdef  __cplusplus
}
#endif

#endif	/* _FS_QUERY_H */
