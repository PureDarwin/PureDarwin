/*
 * Copyright (c) 2013-2017 Apple Inc. All rights reserved.
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

/*
 * IPConfigurationLog.h
 * - logging related functions
 */

#ifndef _S_IPCONFIGURATIONLOG_H
#define _S_IPCONFIGURATIONLOG_H

/* 
 * Modification History
 *
 * March 25, 2013		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <os/log.h>
#include <sys/syslog.h>

#if NO_SYSTEMCONFIGURATION
static inline os_log_type_t
__level_to_type(int level)
{
    os_log_type_t	t;
    if (level < 0) {
	level = ~level;
    }

    switch (level) {
    case LOG_DEBUG :
	t = OS_LOG_TYPE_DEBUG;
	break;
    case LOG_EMERG:
    case LOG_ALERT:
    case LOG_CRIT:
	t = OS_LOG_TYPE_ERROR;
	break;
    case LOG_INFO:
	t = OS_LOG_TYPE_INFO;
	break;
    default:
	t = OS_LOG_TYPE_DEFAULT;
	break;
    }
    return (t);
};

#define	SC_log(__level, __format, ...)					\
    do {								\
	os_log_type_t	__type;						\
									\
	__type = __level_to_type(__level);				\
	os_log_with_type(IPConfigLogGetHandle(), __type,		\
			 __format, ## __VA_ARGS__);			\
    } while (0)
#else /* NO_SYSTEMCONFIGURATION */

#ifndef SC_LOG_HANDLE
#define SC_LOG_HANDLE	IPConfigLogGetHandle
#endif

#include <SystemConfiguration/SCPrivate.h>
#endif /* !NO_SYSTEMCONFIGURATION */

#define kIPConfigurationLogSubsystem	"com.apple.IPConfiguration"

#define kIPConfigurationLogCategoryServer	"Server"
#define kIPConfigurationLogCategoryLibrary	"Library"

void
IPConfigLogSetHandle(os_log_t handle);

os_log_t
IPConfigLogGetHandle(void);

#define IPConfigLog	SC_log
#define IPConfigLogFL	SC_log

#endif /* _S_IPCONFIGURATIONLOG_H */
