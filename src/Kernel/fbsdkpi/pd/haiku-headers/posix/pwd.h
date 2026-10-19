/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PWD_H_
#define _PWD_H_


#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct passwd {
	char	*pw_name;
	char	*pw_passwd;
	uid_t	pw_uid;
	gid_t	pw_gid;
	char	*pw_dir;
	char	*pw_shell;
	char	*pw_gecos;
};

/* traverse the user password database */
extern struct passwd *getpwent(void);
extern int getpwent_r(struct passwd* pwbuf, char* buf, size_t buflen,
				struct passwd** pwbufp);
extern void setpwent(void);
extern void endpwent(void);

/* search the user password database */
extern struct passwd *getpwnam(const char *name);
extern int getpwnam_r(const char *name, struct passwd *passwd, char *buffer,
				size_t bufferSize, struct passwd **result);
extern struct passwd *getpwuid(uid_t uid);
extern int getpwuid_r(uid_t uid, struct passwd *passwd, char *buffer,
				size_t bufferSize, struct passwd **result);

#ifdef __cplusplus
}
#endif

#endif	/* _PWD_H_ */
