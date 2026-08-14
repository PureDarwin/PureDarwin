/*
 * Copyright (c) 2011-2018 Apple Inc. All rights reserved.
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
 * IPConfigurationServiceInternal.h
 * - internal definitions
 */

/* 
 * Modification History
 *
 * April 14, 2011 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#ifndef _IPCONFIGURATIONSERVICEINTERNAL_H
#define _IPCONFIGURATIONSERVICEINTERNAL_H

#include <CoreFoundation/CFString.h>
#include "symbol_scope.h"

#define kIPConfigurationServiceOptions	CFSTR("__IPConfigurationServiceOptions") /* dictionary */

#define _kIPConfigurationServiceOptionClearState	\
    CFSTR("ClearState")		/* boolean */
#define _kIPConfigurationServiceOptionEnableDAD	\
    CFSTR("EnableDAD")		/* boolean */
#define	_kIPConfigurationServiceOptionEnableCLAT46 \
    CFSTR("EnableCLAT46")	/* boolean */
#define _kIPConfigurationServiceOptionMonitorPID	\
    CFSTR("MonitorPID") 	/* boolean */
#define _kIPConfigurationServiceOptionMTU	\
    CFSTR("MTU")		/* number */
#define _kIPConfigurationServiceOptionNoPublish	\
    CFSTR("NoPublish")		/* boolean */
#define _kIPConfigurationServiceOptionPerformNUD	\
    CFSTR("PerformNUD")		/* boolean */
#define _kIPConfigurationServiceOptionServiceID		\
    CFSTR("ServiceID")		/* string (UUID) */
#define _kIPConfigurationServiceOptionAPNName	\
    CFSTR("APNName")		/* string */

#define _kIPConfigurationServiceOptionIPv6Entity	\
    CFSTR("IPv6Entity")	/* dictionary */

#define IPCONFIGURATION_SERVICE_FORMAT CFSTR("Plugin:IPConfigurationService:%@")

INLINE CFStringRef
IPConfigurationServiceKey(CFStringRef serviceID)
{
    return (CFStringCreateWithFormat(NULL, NULL,
				     IPCONFIGURATION_SERVICE_FORMAT,
				     serviceID));
}

#endif /* _IPCONFIGURATIONSERVICEINTERNAL_H */
