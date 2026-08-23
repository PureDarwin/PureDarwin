/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

/*
 * POSIX unnamed semaphores over Grand Central Dispatch.
 *
 * Darwin ships sem_init/sem_destroy as deprecated stubs that always fail with
 * ENOSYS, and PureDarwin's libSystem exports none of the named-semaphore
 * family either. A dispatch semaphore is the platform's real counting
 * semaphore, is exported, and needs no name and no filesystem.
 *
 * Callers declare `sem_t sem;` by value and Darwin's sem_t is an int - too
 * small to hold a dispatch_semaphore_t - so the caller's sem_t stores an index
 * into the table below.
 *
 * This lives in the shared library rather than in the header on purpose. As
 * static inlines with a static table, every translation unit got its own copy
 * of the table: whichever file called sem_init filled *its* table, and a
 * sem_wait from a different file looked up an empty slot, returned -1 without
 * blocking, and callers that ignore the return value ran straight on. In foot
 * that turned into a render worker popping an empty queue and dereferencing
 * NULL.
 *
 * pshared cannot be honoured - a dispatch semaphore lives in one address space
 * - so these are thread-shared only, which is what the ports here use.
 */

#include "epoll.h"

#include <dispatch/dispatch.h>
#include <errno.h>
#include <os/lock.h>
#include <semaphore.h>

static dispatch_semaphore_t pd_sem_table[PD_SEM_MAX];
static int pd_sem_next;
static os_unfair_lock pd_sem_lock = OS_UNFAIR_LOCK_INIT;

static dispatch_semaphore_t
pd_sem_lookup(const sem_t *sem)
{
	dispatch_semaphore_t handle;

	if (sem == NULL || *sem < 0 || *sem >= PD_SEM_MAX) {
		return NULL;
	}

	os_unfair_lock_lock(&pd_sem_lock);
	handle = pd_sem_table[*sem];
	os_unfair_lock_unlock(&pd_sem_lock);

	return handle;
}

int
pd_sem_init(sem_t *sem, int pshared, unsigned int value)
{
	dispatch_semaphore_t handle;
	int slot;

	(void)pshared;

	if (sem == NULL) {
		errno = EINVAL;
		return -1;
	}

	handle = dispatch_semaphore_create((long)value);
	if (handle == NULL) {
		errno = ENOMEM;
		return -1;
	}

	os_unfair_lock_lock(&pd_sem_lock);
	slot = pd_sem_next < PD_SEM_MAX ? pd_sem_next++ : -1;
	if (slot >= 0) {
		pd_sem_table[slot] = handle;
	}
	os_unfair_lock_unlock(&pd_sem_lock);

	if (slot < 0) {
		dispatch_release(handle);
		errno = ENOSPC;
		return -1;
	}

	*sem = slot;
	return 0;
}

int
pd_sem_wait(sem_t *sem)
{
	dispatch_semaphore_t handle = pd_sem_lookup(sem);

	if (handle == NULL) {
		errno = EINVAL;
		return -1;
	}
	dispatch_semaphore_wait(handle, DISPATCH_TIME_FOREVER);
	return 0;
}

int
pd_sem_trywait(sem_t *sem)
{
	dispatch_semaphore_t handle = pd_sem_lookup(sem);

	if (handle == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (dispatch_semaphore_wait(handle, DISPATCH_TIME_NOW) != 0) {
		errno = EAGAIN;
		return -1;
	}
	return 0;
}

int
pd_sem_post(sem_t *sem)
{
	dispatch_semaphore_t handle = pd_sem_lookup(sem);

	if (handle == NULL) {
		errno = EINVAL;
		return -1;
	}
	dispatch_semaphore_signal(handle);
	return 0;
}

int
pd_sem_destroy(sem_t *sem)
{
	dispatch_semaphore_t handle;

	if (sem == NULL || *sem < 0 || *sem >= PD_SEM_MAX) {
		errno = EINVAL;
		return -1;
	}

	os_unfair_lock_lock(&pd_sem_lock);
	handle = pd_sem_table[*sem];
	pd_sem_table[*sem] = NULL;
	os_unfair_lock_unlock(&pd_sem_lock);

	if (handle == NULL) {
		errno = EINVAL;
		return -1;
	}

	dispatch_release(handle);
	return 0;
}
