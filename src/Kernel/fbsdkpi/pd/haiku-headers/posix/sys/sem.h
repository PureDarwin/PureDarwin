/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_SEM_H
#define _SYS_SEM_H


#include <sys/cdefs.h>
#include <sys/ipc.h>
#include <sys/types.h>


/* Semaphore operation flags */
#define SEM_UNDO	10

/* Command definition for semctl */
#define GETPID		3	/* Get process ID of last element manipulating */
#define GETVAL		4	/* Get semval */
#define GETALL		5	/* Get all semval */
#define GETNCNT		6	/* Get semncnt */
#define GETZCNT		7	/* Get semzcnt */
#define SETVAL		8	/* Set semval */
#define SETALL		9	/* Set all semval */

struct semid_ds {
	struct ipc_perm		sem_perm;	/* Operation permission structure */
	unsigned short		sem_nsems;	/* Number of semaphores in set */
	time_t				sem_otime;	/* Last semop */
	time_t				sem_ctime;	/* Last time changed by semctl */
};

/* Structure passed as parameter to the semop function */
struct sembuf {
	unsigned short	sem_num;	/* Semaphore number */
	short			sem_op;		/* Semaphore operation */
	short			sem_flg;	/* Operation flags */
};

/*
 * Semaphore info structure. Useful for the ipcs
 * standard utily
 */
struct seminfo {
	int	semmni;	/* Number of semaphore identifies */
	int semmns;	/* Number of semaphore in system */
	int semmnu;	/* Number of undo structures in system */
	int semmsl;	/* Max number of semaphores per id */
	int semopm;	/* Max number of operations per semop call */
	int semume;	/* Max number of undo entries per process */
	int semusz;	/* Size in bytes of undo structure */
	int semvmx;	/* Semaphore maximum valure */
	int semaem;	/* adjust on exit max value */
};


__BEGIN_DECLS

int		semctl(int semID, int semNum, int command, ...);
int		semget(key_t key, int numSems, int semFlags);
int		semop(int semID, struct sembuf *semOps, size_t numSemOps);

__END_DECLS

#endif	/* _SYS_SEM_H */
