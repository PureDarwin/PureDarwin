// SPDX-License-Identifier: MIT
// FreeBSD's sys/ck.h, concurrency kit in the kernel and queue.h outside it
#ifndef _SYS_CK_H_
#define _SYS_CK_H_

#ifdef _KERNEL
#include <ck_queue.h>
#include <ck_epoch.h>
#else
#include <sys/queue.h>
#define CK_STAILQ_HEAD STAILQ_HEAD
#define CK_STAILQ_ENTRY STAILQ_ENTRY
#define CK_LIST_HEAD LIST_HEAD
#define CK_LIST_ENTRY LIST_ENTRY
#define CK_SLIST_HEAD SLIST_HEAD
#define CK_SLIST_ENTRY SLIST_ENTRY
#endif

#endif // !_SYS_CK_H_
