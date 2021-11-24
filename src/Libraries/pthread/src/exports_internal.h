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

#ifndef __LIBPTHREAD_EXPORTS_INTERNAL_H__
#define __LIBPTHREAD_EXPORTS_INTERNAL_H__

/*!
 * @file exports_internal.h
 *
 * @brief
 * This file has prototypes for symbols / functions that are exported
 * without a public header.
 *
 * @discussion
 * This header is also fed to TAPI and needs to work without internal.h
 */

#include <os/base.h>
#include <mach/mach.h>
#include <pthread.h>

struct ProgramVars;

OS_EXPORT int __is_threaded;

OS_EXPORT const int __unix_conforming;

OS_EXPORT
void
_pthread_set_self(pthread_t);

OS_EXPORT
pthread_t
_pthread_self(void);

OS_EXPORT
int
__pthread_init(const struct _libpthread_functions *pthread_funcs,
		const char *envp[], const char *apple[],
		const struct ProgramVars *vars);

OS_EXPORT OS_NORETURN
void
thread_start(pthread_t self, mach_port_t kport,
		void *(*fun)(void *), void *arg,
		size_t stacksize, unsigned int flags); // trampoline into _pthread_start

OS_EXPORT OS_NORETURN
void
_pthread_start(pthread_t thread, mach_port_t kport,
		void *(*fun)(void *), void *arg,
		size_t stacksize, unsigned int flags);

OS_EXPORT OS_NORETURN
void
start_wqthread(pthread_t self, mach_port_t kport,
		void *stackaddr, void *unused, int reuse); // trampoline into _start_wqthread

OS_EXPORT
void
_pthread_wqthread(pthread_t self, mach_port_t kport,
		void *stackaddr, void *keventlist, int flags, int nkevents);

#if defined(__x86_64__) || defined(__i386__) || defined(__arm64__)
OS_EXPORT
void
___chkstk_darwin(void);

OS_EXPORT
void
thread_chkstk_darwin(void);
#endif //  defined(__x86_64__) || defined(__i386__) || defined(__arm64__)

#pragma mark - exports with prototypes from not in libpthread

OS_EXPORT
int
sigwait(const sigset_t *, int *) __DARWIN_ALIAS_C(sigwait);

#pragma mark - shared with libsystem_kernel.dylib
/* Note: these can't use _pthread_malloc / _pthread_free unconditionally */

OS_EXPORT
void
_pthread_clear_qos_tsd(mach_port_t kport);

OS_EXPORT
void
_pthread_exit_if_canceled(int error);

#pragma mark - atfork libSystem integration

OS_EXPORT void _pthread_atfork_prepare_handlers(void);
OS_EXPORT void _pthread_atfork_prepare(void);
OS_EXPORT void _pthread_atfork_parent(void);
OS_EXPORT void _pthread_atfork_parent_handlers(void);
OS_EXPORT void _pthread_atfork_child(void);
OS_EXPORT void _pthread_atfork_child_handlers(void);
OS_EXPORT void _pthread_fork_prepare(void);
OS_EXPORT void _pthread_fork_parent(void);
OS_EXPORT void _pthread_fork_child(void);
OS_EXPORT void _pthread_fork_child_postinit(void);

#pragma mark - TAPI
#ifdef __clang_tapi__

#define declare_symbol(s) OS_EXPORT void __tapi_##s(void) asm("_" #s)

#if TARGET_OS_OSX && defined(__i386__)
// TAPI will see the $UNIX2003 redirected symbols
declare_symbol(pthread_cancel);
declare_symbol(pthread_cond_init);
declare_symbol(pthread_cond_timedwait);
declare_symbol(pthread_cond_wait);
declare_symbol(pthread_join);
declare_symbol(pthread_mutexattr_destroy);
declare_symbol(pthread_rwlock_destroy);
declare_symbol(pthread_rwlock_init);
declare_symbol(pthread_rwlock_rdlock);
declare_symbol(pthread_rwlock_tryrdlock);
declare_symbol(pthread_rwlock_trywrlock);
declare_symbol(pthread_rwlock_unlock);
declare_symbol(pthread_rwlock_wrlock);
declare_symbol(pthread_setcancelstate);
declare_symbol(pthread_setcanceltype);
declare_symbol(pthread_sigmask);
declare_symbol(pthread_testcancel);
declare_symbol(sigwait);
// TAPI will see the $NOCANCEL$UNIX2003 redirected symbols
declare_symbol(pthread_cond_timedwait$UNIX2003);
declare_symbol(pthread_cond_wait$UNIX2003);
declare_symbol(pthread_join$UNIX2003);
declare_symbol(sigwait$UNIX2003);
#else
// TAPI will see the $NOCANCEL redirected symbols
declare_symbol(pthread_cond_timedwait);
declare_symbol(pthread_cond_wait);
declare_symbol(pthread_join);
declare_symbol(sigwait);
#endif

#undef declare_symbol

#endif // __clang_tapi__
#endif // __LIBPTHREAD_EXPORTS_INTERNAL_H__
