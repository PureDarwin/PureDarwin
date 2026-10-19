/*
 * Copyright 2023, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_CONDVAR_H_
#define _FBSD_COMPAT_SYS_CONDVAR_H_


#include <sys/param.h>
#include <KernelExport.h>


__BEGIN_DECLS


struct cv {
	// We cannot include <condition_variable.h> here as it is C++-only.
	char condition[roundup((sizeof(void*) * 5) + sizeof(spinlock) + sizeof(int32), sizeof(void*))];
};

#ifdef __cplusplus
#	define __cv_ConditionVariable(CV) reinterpret_cast<ConditionVariable*>(&(CV)->condition)
#endif


void cv_init(struct cv*, const char*);
void cv_destroy(struct cv*);

void cv_wait(struct cv*, struct mtx*);
int cv_timedwait(struct cv*, struct mtx*, int);
int cv_wait_sig(struct cv*, struct mtx*);
void cv_signal(struct cv*);
void cv_broadcast(struct cv*);


__END_DECLS


#endif /* _FBSD_COMPAT_SYS_CONDVAR_H_ */
