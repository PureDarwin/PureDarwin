/*
 * Copyright 2004-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _GRP_H_
#define _GRP_H_


#include <sys/types.h>


struct group {
	char	*gr_name;
	char	*gr_passwd;
	gid_t	gr_gid;
	char	**gr_mem;
};


#ifdef __cplusplus
extern "C" {
#endif

extern struct group *getgrgid(gid_t gid);
extern struct group *getgrnam(const char *name);
extern int getgrgid_r(gid_t gid, struct group *group, char *buffer,
				size_t bufferSize, struct group **_result);
extern int getgrnam_r(const char *name, struct group *group, char *buffer,
				size_t bufferSize, struct group **_result);

extern struct group *getgrent(void);
extern int getgrent_r(struct group* group, char* buffer, size_t bufferSize,
				struct group** _result);
extern void setgrent(void);
extern void endgrent(void);

#ifdef __cplusplus
}
#endif

#endif /* _GRP_H_ */
