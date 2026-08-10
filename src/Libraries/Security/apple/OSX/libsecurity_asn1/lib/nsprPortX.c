/*
 * Copyright (c) 2003-2006,2008,2010-2012 Apple Inc. All Rights Reserved.
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
 *
 * nsprPortX.c - minimal platform dependent NSPR functions to enable
 *               use of DER libraries
 */

#ifndef _NSPR_PORT_X_H_
#define _NSPR_PORT_X_H_

#include "prmem.h"
#include "prlock.h"
#include "prerror.h"
#include "prinit.h"
#include "prbit.h"

#include <stdlib.h>
#include <pthread.h>
#include <string.h>

// MARK: *** Memory ***

NSPR_API(void *) PR_Malloc(PRSize size)
{
	return malloc(size ? size : 1);
}
NSPR_API(void *) PR_Calloc(PRSize nelem, PRSize elsize)
{
	return calloc(nelem, elsize);
}
NSPR_API(void *) PR_Realloc(void *ptr, PRSize size)
{
	return realloc(ptr, size);
}
NSPR_API(void) PR_Free(void *ptr)
{
	return free(ptr);
}

// MARK: *** locks ***

NSPR_API(PRLock*) PR_NewLock(void)
{
	pthread_mutex_t *pm = PR_Malloc(sizeof(pthread_mutex_t));
	if(pm == NULL) {
		return NULL;
	}
	if(pthread_mutex_init(pm, NULL)) {
		PR_Free(pm);
		return NULL;
	}
	return (PRLock*)pm;
}

NSPR_API(void) PR_DestroyLock(PRLock *lock)
{
	if(lock == NULL) {
		return;
	}
	pthread_mutex_destroy((pthread_mutex_t *)lock);
	PR_Free(lock);
}

NSPR_API(void) PR_Lock(PRLock *lock)
{
	if(lock == NULL) {
		return;
	}
	pthread_mutex_lock((pthread_mutex_t *)lock);
}

NSPR_API(PRStatus) PR_Unlock(PRLock *lock)
{
	if(lock == NULL) {
		return PR_FAILURE;
	}
	pthread_mutex_unlock((pthread_mutex_t *)lock);
	return PR_SUCCESS;
}

// MARK: *** get/set error ***

/* 
 * key for pthread_{set,get}specific and a lock to ensure it gets 
 * created once 
 */
static pthread_key_t PR_threadKey;
static int PR_threadKeyInitFlag;		// we have a PR_threadKey
static int PR_threadKeyErrorFlag;		// unable to create PR_threadKey
static pthread_mutex_t PR_threadKeyLock = PTHREAD_MUTEX_INITIALIZER;

/*
 * The thing that gets stored on a per-thread basis. A pointer to 
 * this is associated with key PR_threadKey. Mallocd in 
 * PR_getThreadErrInfo(); freed directly by free() as 
 * PR_threadKey's destructor. 
 */
typedef struct {
	PRInt32 		osError;
	PRErrorCode 	prError;
} PR_threadErrInfo;

/* 
 * One-time init of PR_threadKey, returns nonzero on error.
 * Does not attempt to init PR_threadKey if doCreate is false and
 * a previous call to this routine resulted in error (i.e., this
 * is the GetXError() following a failed SetError()).
 */
static PRInt32 PR_initThreadKey(
	int doCreate)
{
	PRInt32 prtn = 0;
	if(PR_threadKeyInitFlag) {
		/* thread safe since we never clear this flag; we're ready to go */
		return 0;
	}
	pthread_mutex_lock(&PR_threadKeyLock);
	if(PR_threadKeyErrorFlag && !doCreate) {
		/* no error to get because the last SetXError failed */
		prtn = PR_IO_ERROR;
	}
	else if(!PR_threadKeyInitFlag) {
		prtn = pthread_key_create(&PR_threadKey, free);
		if(prtn) {
			/* out of pthread_key_t's */
			PR_threadKeyErrorFlag = 1;
		}
		else {
			PR_threadKeyErrorFlag = 0;	// in case of retry */
			PR_threadKeyInitFlag = 1;	// success
		}
	}
	pthread_mutex_unlock(&PR_threadKeyLock);
	return prtn;
}

/*
 * Get current thread's PR_threadErrInfo. Create one if doCreate is
 * true and one does not exist. 
 *
 * -- A nonzero *threadKeyError on return indicates that we can 
 *    not create a pthread_key_t; in this case we return NULL. 
 * -- Note that NULL return with zero threadKeyError and zero
 *    doCreate indicates "no per-thread error set yet", which is
 *    not an error. 
 */
static PR_threadErrInfo *PR_getThreadErrInfo(
	int doCreate,
	PRInt32 *threadKeyError)		// RETURNED, an OSStatus
{
	*threadKeyError = PR_initThreadKey(doCreate);
	if(*threadKeyError) {
		return NULL;
	}
	PR_threadErrInfo *errInfo = pthread_getspecific(PR_threadKey);
	if((errInfo == NULL) && doCreate) {
		errInfo = (PR_threadErrInfo *)malloc(sizeof(*errInfo));
		if(errInfo == NULL) {
			/* 
			 * malloc failure, retriable failure of this routine (not
			 * a PR_threadKeyErrorFlag style error).
			 * Note that this is *not* detected in a subsequent
			 * GetXError() call, but it will allow for somewhat
			 * graceful recovery in case some memory gets freed
			 * up. 
			 */
			*threadKeyError = PR_OUT_OF_MEMORY_ERROR;
		}
		else {
			memset(errInfo, 0, sizeof(*errInfo));
			pthread_setspecific(PR_threadKey, errInfo);
		}
	}
	return errInfo;
}

PR_IMPLEMENT(PRErrorCode) PR_GetError(void)
{
	PRInt32 prtn;
	PR_threadErrInfo *errInfo = PR_getThreadErrInfo(0, &prtn);
	if(errInfo == NULL) {
		/* no error set or per-thread logic uninitialized */
		if(prtn) {
			return PR_INSUFFICIENT_RESOURCES_ERROR;
		}
		else {
			return 0;
		}
	}
	else {
		return errInfo->prError;
	}
}

PR_IMPLEMENT(PRInt32) PR_GetOSError(void)
{
	PRInt32 prtn;
	PR_threadErrInfo *errInfo = PR_getThreadErrInfo(0, &prtn);
	if(errInfo == NULL) {
		/* no error set or per-thread logic uninitialized */
		return prtn;
	}
	else {
		return errInfo->osError;
	}
}

PR_IMPLEMENT(void) PR_SetError(PRErrorCode code, PRInt32 osErr)
{
	PRInt32 prtn;
	PR_threadErrInfo *errInfo = PR_getThreadErrInfo(1, &prtn);
	if(errInfo != NULL) {
		errInfo->osError = osErr;
		errInfo->prError = code;
	}
	/* else per-thread logic uninitialized */
}

// MARK: *** misc. ***

/*
** Compute the log of the least power of 2 greater than or equal to n
*/
NSPR_API(PRIntn) PR_CeilingLog2(PRUint32 i)
{
	PRIntn r;
	PR_CEILING_LOG2(r,i);
	return r;
}

#endif	/* _NSPR_PORT_X_H_ */
