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

#ifndef __SUBSYSTEM_H__
#define __SUBSYSTEM_H__

#include <sys/stat.h>

__BEGIN_DECLS
/*
 * Returns an fd for the given path, relative to root or to
 * the subsystem root for the process.  Behaves exactly like
 * open in every way, except O_CREAT is forbidden.
 *
 * Returns a file descriptor on success, or -1 on failure.
 * errno is set exactly as open would have set it, except
 * that O_CREAT will result in EINVAL.
 */
int open_with_subsystem(const char * path, int oflag);

/*
 * Invokes stat for the given path, relative to root or to
 * the subsystem root for the process.  Behaves exactly like
 * stat in every way.
 *
 * Returns 0 on success, or -1 on failure.  On failure, errno
 * is set exactly as stat would have set it.
 */
int stat_with_subsystem(const char *__restrict path, struct stat *__restrict buf);
__END_DECLS

#endif /* __SUBSYSTEM_H__ */
