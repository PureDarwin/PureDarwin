/*
 * Copyright 2020 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _UTMPX_H_
#define _UTMPX_H_


#include <sys/time.h>
#include <sys/types.h>


struct utmpx {
	short 			ut_type;		/* type of entry */
	struct timeval	ut_tv;			/* modification time */
	char			ut_id[8];		/* entry identifier */
	pid_t			ut_pid;			/* process ID */
	char			ut_user[32];	/* user login name */
	char			ut_line[16];	/* device name */
	char			ut_host[128];	/* remote hostname */
	char			__ut_reserved[64];
};


#define	EMPTY			0	/* No valid user accounting information. */
#define	BOOT_TIME		1	/* Identifies time of system boot. */
#define	OLD_TIME		2	/* Identifies time when system clock changed. */
#define	NEW_TIME		3	/* Identifies time after system clock changed. */
#define	USER_PROCESS	4	/* Identifies a process. */
#define	INIT_PROCESS	5	/* Identifies a process spawned by the init process. */
#define	LOGIN_PROCESS	6	/* Identifies the session leader of a logged-in user. */
#define	DEAD_PROCESS	7	/* Identifies a session leader who has exited. */


#ifdef __cplusplus
extern "C" {
#endif

void endutxent(void);
struct utmpx* getutxent(void);
struct utmpx* getutxid(const struct utmpx *);
struct utmpx* getutxline(const struct utmpx *);
struct utmpx* pututxline(const struct utmpx *);
void setutxent(void);

#ifdef __cplusplus
}
#endif

#endif	/* _UTMPX_H_ */
