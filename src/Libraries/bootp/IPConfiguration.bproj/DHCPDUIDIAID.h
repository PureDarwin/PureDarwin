/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
 * DHCPDUIDIAID.h
 * - routines to set/access the DHCP client DUID and the IAIDs for particular
 *   interfaces
 */

#ifndef _S_DHCPDUIDIAID_H
#define _S_DHCPDUIDIAID_H


/* 
 * Modification History
 *
 * May 14, 2010
 * - created
 */

#include <CoreFoundation/CFData.h>
#include <stdint.h>
#include <stdbool.h>
#include "symbol_scope.h"
#include "interfaces.h"

typedef uint32_t	DHCPIAID;

PRIVATE_EXTERN CFDataRef
DHCPDUIDGet(interface_list_t * interfaces);

PRIVATE_EXTERN DHCPIAID
DHCPIAIDGet(const char * ifname);

#endif /* _S_DHCPDUIDIAID_H */
