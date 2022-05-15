/*
 * Copyright (c) 2013-19 Apple, Inc. All rights reserved.
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

#ifndef _COPYFILE_PRIVATE_H
# define _COPYFILE_PRIVATE_H

/*
 * Set (or get) the intent type; see xattr_properties.h for details.
 * This command uses a pointer to CopyOperationIntent_t as the parameter.
 */
# define COPYFILE_STATE_INTENT	256

/*
 * File flags that are not preserved when copying stat information.
 */
#define COPYFILE_OMIT_FLAGS 	(UF_TRACKED | SF_RESTRICTED | SF_NOUNLINK | UF_DATAVAULT)

/*
 * File flags that are not removed when replacing an existing file.
 */
#define COPYFILE_PRESERVE_FLAGS	(SF_RESTRICTED | SF_NOUNLINK | UF_DATAVAULT)

#endif /* _COPYFILE_PRIVATE_H */
