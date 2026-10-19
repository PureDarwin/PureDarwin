/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_IPC_H
#define _SYS_IPC_H


#include <sys/cdefs.h>
#include <sys/types.h>


/* Mode bits for msgget(), semget(), and shmget() */
#define IPC_CREAT	01000	/* create key */
#define IPC_EXCL	02000	/* fail if key exists */
#define IPC_NOWAIT	04000	/* do not block */

/* Control commands for msgctl(), semctl(), and shmctl() */
#define IPC_RMID	0		/* remove identifier */
#define IPC_SET		1		/* set options */
#define IPC_STAT	2		/* get options */

/* Private key */
#define IPC_PRIVATE		(key_t)0


struct ipc_perm {
	key_t	key;			/* IPC identifier */
	uid_t	uid;			/* owner's user ID */
	gid_t	gid;			/* owner's group ID */
	uid_t	cuid;			/* creator's user ID */
	gid_t	cgid;			/* creator's group ID */
	mode_t	mode;			/* Read/write permission */
};


__BEGIN_DECLS

key_t ftok(const char *path, int id);

__END_DECLS

#endif	/* _SYS_IPC_H */
