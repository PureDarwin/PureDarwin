/*
 * Copyright 2008-2015 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_


#include <fcntl.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <time.h>


typedef struct _sem_t {
	int32_t type;
	union {
		int32_t named_sem_id;
		int32_t unnamed_sem;
	} u;
	int32_t padding[2];
} sem_t;

#define SEM_FAILED		((sem_t*)(long)-1)
#define SEM_VALUE_MAX	INT_MAX

__BEGIN_DECLS

sem_t*	sem_open(const char* name, int openFlags,...);
int		sem_close(sem_t* semaphore);
int		sem_unlink(const char* name);

int		sem_init(sem_t* semaphore, int shared, unsigned value);
int		sem_destroy(sem_t* semaphore);

int		sem_post(sem_t* semaphore);
int		sem_clockwait(sem_t* semaphore, clockid_t clock_id,
			const struct timespec* abstime);
int		sem_timedwait(sem_t* semaphore, const struct timespec* abstime);
int		sem_trywait(sem_t* semaphore);
int		sem_wait(sem_t* semaphore);
int		sem_getvalue(sem_t* semaphore, int* value);

__END_DECLS


#endif	/* _SEMAPHORE_H_ */
