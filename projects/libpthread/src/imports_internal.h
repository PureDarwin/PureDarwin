/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __LIBPTHREAD_IMPORTS_INTERNAL_H__
#define __LIBPTHREAD_IMPORTS_INTERNAL_H__

/*!
 * @file imports_internal.h
 *
 * @brief
 * This file lists prototypes that do not have a header on the system,
 * like syscalls, that we need to import in pthread.
 */

#include <os/base.h>
#include <mach/mach.h>
#include <sys/time.h>
#include <stack_logging.h>

extern boolean_t swtch_pri(int);

// Defined in libsyscall; initialized in libmalloc
extern malloc_logger_t *__syscall_logger;

// syscalls

extern uint32_t __psynch_mutexwait(pthread_mutex_t *mutex,  uint32_t mgen, uint32_t  ugen, uint64_t tid, uint32_t flags);
extern uint32_t __psynch_mutexdrop(pthread_mutex_t *mutex,  uint32_t mgen, uint32_t  ugen, uint64_t tid, uint32_t flags);

extern uint32_t __psynch_cvbroad(pthread_cond_t *cv, uint64_t cvlsgen, uint64_t cvudgen, uint32_t flags, pthread_mutex_t *mutex,  uint64_t mugen, uint64_t tid);
extern uint32_t __psynch_cvsignal(pthread_cond_t *cv, uint64_t cvlsgen, uint32_t cvugen, int thread_port, pthread_mutex_t *mutex,  uint64_t mugen, uint64_t tid, uint32_t flags);
extern uint32_t __psynch_cvwait(pthread_cond_t *cv, uint64_t cvlsgen, uint32_t cvugen, pthread_mutex_t *mutex,  uint64_t mugen, uint32_t flags, int64_t sec, uint32_t nsec);
extern uint32_t __psynch_cvclrprepost(void *cv, uint32_t cvgen, uint32_t cvugen, uint32_t cvsgen, uint32_t prepocnt, uint32_t preposeq, uint32_t flags);

extern uint32_t __psynch_rw_longrdlock(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_yieldwrlock(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern int      __psynch_rw_downgrade(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_upgrade(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_rdlock(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_wrlock(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_unlock(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);
extern uint32_t __psynch_rw_unlock2(pthread_rwlock_t *rwlock, uint32_t lgenval, uint32_t ugenval, uint32_t rw_wc, int flags);

extern uint32_t __bsdthread_ctl(uintptr_t cmd, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
extern pthread_t __bsdthread_create(void *(*func)(void *), void *func_arg, void *stack, pthread_t  thread, unsigned int flags);
extern int      __bsdthread_register(void (*)(pthread_t, mach_port_t, void *(*)(void *), void *, size_t, unsigned int), void (*)(pthread_t, mach_port_t, void *, void *, int), int,void (*)(pthread_t, mach_port_t, void *(*)(void *), void *, size_t, unsigned int), int32_t *,__uint64_t);
extern int      __bsdthread_terminate(void *freeaddr, size_t freesize, mach_port_t kport, mach_port_t joinsem);

extern uint64_t __thread_selfid(void);
extern int      __disable_threadsignal(int);

extern int      __pthread_canceled(int);
extern int      __pthread_chdir(const char *path);
extern int      __pthread_fchdir(int fd);
extern int      __pthread_kill(mach_port_t, int);
extern int      __pthread_markcancel(mach_port_t);
extern int      __pthread_sigmask(int, const sigset_t *, sigset_t *);

extern int      __gettimeofday(struct timeval *, struct timezone *);
extern void     __exit(int) __attribute__((noreturn));
extern int      __proc_info(int callnum, int pid, int flavor, uint64_t arg, void *buffer, int buffersize);
extern int      __semwait_signal_nocancel(int, int, int, int, __int64_t, __int32_t);
extern int      __sigwait(const sigset_t *set, int *sig);
extern int      __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);


#endif // __LIBPTHREAD_IMPORTS_INTERNAL_H__
