/*
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#if defined(__MWERKS__) && !defined(__private_extern__)
#define __private_extern__ __declspec(private_extern)
#endif

#include <stdint.h>
#include <string.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <uuid/uuid.h>
#else
#include <mach-o/loader.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */

/*
 * uuid() is called to set the uuid[] bytes for the uuid load command.
 */
__private_extern__
void
uuid(
uint8_t *uuid)
{
#if defined(KLD) && defined(__STATIC__)
    memset(uuid, '\0', sizeof(struct uuid_command));
#else 
    uuid_generate_random((void *)uuid);
#endif 
}
