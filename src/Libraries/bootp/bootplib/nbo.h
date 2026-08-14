/*
 * Copyright (c) 2009-2011 Apple Inc. All rights reserved.
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
 * nbo.h
 * - network byte order
 * - inlines to set/get values to/from network byte order
 */

#ifndef _S_NBO_H
#define _S_NBO_H

#include "symbol_scope.h"
#include <strings.h>

/*
 * Function: net_uint16_set
 * Purpose:
 *   Set a field in a structure that's at least 16 bits to the given
 *   value, putting it into network byte order
 */
INLINE void
net_uint16_set(uint8_t * field, uint16_t value)
{
    uint16_t tmp_value = htons(value);
    bcopy((void *)&tmp_value, (void *)field,
	  sizeof(uint16_t));
    return;
}

/*
 * Function: net_uint16_get
 * Purpose:
 *   Get a field in a structure that's at least 16 bits, converting
 *   to host byte order.
 */
INLINE uint16_t
net_uint16_get(const uint8_t * field)
{
    uint16_t tmp_field;

    bcopy((void *)field, (void *)&tmp_field, 
	  sizeof(uint16_t));
    return (ntohs(tmp_field));
}

/*
 * Function: net_uint32_set
 * Purpose:
 *   Set a field in a structure that's at least 32 bits to the given
 *   value, putting it into network byte order
 */
INLINE void
net_uint32_set(uint8_t * field, uint32_t value)
{
    uint32_t tmp_value = htonl(value);
    
    bcopy((void *)&tmp_value, (void *)field, 
	  sizeof(uint32_t));
    return;
}

/*
 * Function: net_uint32_get
 * Purpose:
 *   Get a field in a structure that's at least 32 bits, converting
 *   to host byte order.
 */
INLINE uint32_t
net_uint32_get(const uint8_t * field)
{
    uint32_t tmp_field;

    bcopy((void *)field, &tmp_field, 
	  sizeof(uint32_t));
    return (ntohl(tmp_field));
}

#endif /* _S_NBO_H */
