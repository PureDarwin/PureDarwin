/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_MSG_H
#define _SYS_MSG_H


#include <config/types.h>

#include <sys/cdefs.h>
#include <sys/ipc.h>
#include <sys/types.h>

typedef __haiku_uint32 msgqnum_t;
typedef __haiku_uint32 msglen_t;

/* No error if big message */
#define MSG_NOERROR		010000

struct msqid_ds {
	struct ipc_perm		msg_perm;	/* Operation permission structure */
	msgqnum_t			msg_qnum;	/* Number of messages currently on queue */
	msglen_t			msg_qbytes;	/* Max number of bytes allowed on queue */
	pid_t				msg_lspid;	/* PID of last msgsnd */
	pid_t				msg_lrpid;	/* PID of last msgrcv */
	time_t				msg_stime;	/* Time of last msgsnd */
	time_t				msg_rtime;	/* Time of last msgrcv */
	time_t				msg_ctime;	/* Time of last change */
};

/* Structure used to send/receive a message */
struct msgbuf {
	long	mtype;		/* message type */
	char	mtext[1];	/* message text */
};

__BEGIN_DECLS

int			msgctl(int, int, struct msqid_ds *);
int			msgget(key_t, int);
ssize_t		msgrcv(int, void *, size_t, long, int);
int			msgsnd(int, const void *, size_t, int);

__END_DECLS

#endif	/* _SYS_MSG_H */
