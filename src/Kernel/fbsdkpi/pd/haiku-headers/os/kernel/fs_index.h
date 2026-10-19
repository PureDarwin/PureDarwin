/*
 * Copyright 2002-2003, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FS_INDEX_H
#define	_FS_INDEX_H


#include <OS.h>
#include <dirent.h>


typedef struct index_info {
	uint32	type;
	off_t	size;
	time_t	modification_time;
	time_t	creation_time;
	uid_t	uid;
	gid_t	gid;
} index_info;


#ifdef  __cplusplus
extern "C" {
#endif

extern int		fs_create_index(dev_t device, const char *name, uint32 type, uint32 flags);
extern int		fs_remove_index(dev_t device, const char *name);
extern int		fs_stat_index(dev_t device, const char *name, struct index_info *indexInfo);

extern DIR		*fs_open_index_dir(dev_t device);
extern int		fs_close_index_dir(DIR *indexDirectory);
extern struct dirent *fs_read_index_dir(DIR *indexDirectory);
extern void		fs_rewind_index_dir(DIR *indexDirectory);

#ifdef  __cplusplus
}
#endif

#endif	/* _FS_INDEX_H */
