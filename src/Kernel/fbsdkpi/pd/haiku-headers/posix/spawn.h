/*
 * Copyright 2017 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SPAWN_H_
#define _SPAWN_H_


#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>


/*
 * Flags for spawn attributes.
 */
#define POSIX_SPAWN_RESETIDS		0x01
#define POSIX_SPAWN_SETPGROUP		0x02
#if 0	/* Unimplemented flags: */
#define POSIX_SPAWN_SETSCHEDPARAM	0x04
#define POSIX_SPAWN_SETSCHEDULER	0x08
#endif	/* 0 */
#define POSIX_SPAWN_SETSIGDEF		0x10
#define POSIX_SPAWN_SETSIGMASK		0x20
#define POSIX_SPAWN_SETSID			0x40


typedef struct _posix_spawnattr	*posix_spawnattr_t;
typedef struct _posix_spawn_file_actions	*posix_spawn_file_actions_t;


#ifdef __cplusplus
extern "C" {
#endif


extern int posix_spawn(pid_t *pid, const char *path,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *attrp, char *const argv[], char *const envp[]);
extern int posix_spawnp(pid_t *pid, const char *file,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *attrp, char *const argv[],
	char *const envp[]);

/* file actions functions */
extern int posix_spawn_file_actions_init(
	posix_spawn_file_actions_t *file_actions);
extern int posix_spawn_file_actions_destroy(
	posix_spawn_file_actions_t *file_actions);
extern int posix_spawn_file_actions_addopen(
	posix_spawn_file_actions_t *file_actions,
	int fildes, const char *path, int oflag, mode_t mode);
extern int posix_spawn_file_actions_addclose(
	posix_spawn_file_actions_t *file_actions, int fildes);
extern int posix_spawn_file_actions_adddup2(
	posix_spawn_file_actions_t *file_actions, int fildes, int newfildes);
extern int posix_spawn_file_actions_addchdir(
	posix_spawn_file_actions_t *file_actions, const char *path);
extern int posix_spawn_file_actions_addfchdir(
	posix_spawn_file_actions_t *file_actions, int fildes);

/* spawn attribute functions */
extern int posix_spawnattr_destroy(posix_spawnattr_t *attr);
extern int posix_spawnattr_init(posix_spawnattr_t *attr);
extern int posix_spawnattr_getflags(const posix_spawnattr_t *attr,
	short *_flags);
extern int posix_spawnattr_setflags(posix_spawnattr_t *attr, short flags);
extern int posix_spawnattr_getpgroup(const posix_spawnattr_t *attr,
	pid_t *_pgroup);
extern int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgroup);
extern int posix_spawnattr_getsigdefault(const posix_spawnattr_t *attr,
	sigset_t *sigdefault);
extern int posix_spawnattr_setsigdefault(posix_spawnattr_t *attr,
	const sigset_t *sigdefault);
extern int posix_spawnattr_getsigmask(const posix_spawnattr_t *attr,
	sigset_t *_sigmask);
extern int posix_spawnattr_setsigmask(posix_spawnattr_t *attr,
	const sigset_t *sigmask);


#ifdef __cplusplus
}
#endif


#endif	/* _SPAWN_H_ */

