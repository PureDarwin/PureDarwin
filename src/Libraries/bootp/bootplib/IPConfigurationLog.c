/*
 * Copyright (c) 2013-2016 Apple Inc. All rights reserved.
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
 * IPConfigurationLog.c
 * - logging related functions
 */

/* 
 * Modification History
 *
 * March 25, 2013		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include "IPConfigurationLog.h"
#include "symbol_scope.h"

STATIC os_log_t	S_log_handle;

PRIVATE_EXTERN void
IPConfigLogSetHandle(os_log_t handle)
{
    S_log_handle = handle;
    return;
}

PRIVATE_EXTERN os_log_t
IPConfigLogGetHandle(void)
{
    return (S_log_handle);
}
